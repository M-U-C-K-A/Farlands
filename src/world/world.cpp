#include "world.h"
#include "block.h"
#include "chunk_serializer.h"

#include <cmath>
#include <filesystem>
#include "core/logger.h"

// ── Chunk Access ────────────────────────────────────────────────
Chunk *World::getChunk(int cx, int cz)
{
	auto it = m_chunks.find(glm::ivec2(cx, cz));
	return it != m_chunks.end() ? it->second.get() : nullptr;
}

const Chunk *World::getChunk(int cx, int cz) const
{
	auto it = m_chunks.find(glm::ivec2(cx, cz));
	return it != m_chunks.end() ? it->second.get() : nullptr;
}

// ── World-space Block Query ─────────────────────────────────────
BlockType World::getBlockAt(int wx, int wy, int wz) const
{
	if(wy < 0 || wy >= CHUNK_SIZE_Y)
		return BlockType::Air;

	int cx = (wx >= 0) ? (wx / CHUNK_SIZE_X)
					   : ((wx - CHUNK_SIZE_X + 1) / CHUNK_SIZE_X);
	int cz = (wz >= 0) ? (wz / CHUNK_SIZE_Z)
					   : ((wz - CHUNK_SIZE_Z + 1) / CHUNK_SIZE_Z);

	int lx = wx - cx * CHUNK_SIZE_X;
	int lz = wz - cz * CHUNK_SIZE_Z;

	const Chunk *chunk = getChunk(cx, cz);
	if(!chunk)
		return BlockType::Air;

	return chunk->getBlock(lx, wy, lz);
}

// ── Set World Directory ─────────────────────────────────────────
void World::setWorldDir(const std::string &dir)
{
	m_worldDir = dir;
	std::filesystem::create_directories(m_worldDir);
}

// ── Terrain Generation (Biome-driven) ───────────────────────────
void World::generateTerrain(Chunk &chunk)
{
	auto pos = chunk.getChunkPos();

	for(int x = 0; x < CHUNK_SIZE_X; ++x)
		{
			for(int z = 0; z < CHUNK_SIZE_Z; ++z)
				{
					float wx = static_cast<float>(pos.x * CHUNK_SIZE_X + x);
					float wz = static_cast<float>(pos.y * CHUNK_SIZE_Z + z);

					// Obtenir la hauteur et le biome via le BiomeManager
					int height = m_biomeManager.getHeightAt(wx, wz);
					BiomeType biome = m_biomeManager.getBiomeAt(wx, wz);
					const BiomeData &bd = BiomeManager::getData(biome);

					for(int y = 0; y < height; ++y)
						{
							if(y == 0)
								{
									chunk.setBlock(x, y, z,
												   BlockType::Bedrock);
								}
							else if(y < height - bd.subSurfaceDepth)
								{
									chunk.setBlock(x, y, z, BlockType::Stone);
								}
							else if(y < height - 1)
								{
									chunk.setBlock(x, y, z, bd.subSurface);
								}
							else
								{
									chunk.setBlock(x, y, z, bd.surfaceBlock);
								}
						}

					// Eau dans les océans et lacs (remplir jusqu'au
					// niveau de la mer = 30)
					int seaLevel = 30;
					if(height < seaLevel)
						{
							for(int y = height; y < seaLevel; ++y)
								{
									chunk.setBlock(x, y, z, BlockType::Water);
								}
						}
				}
		}
}

// ── Update Around Player (Infinite World) ───────────────────────
bool World::updateAroundPlayer(int playerChunkX, int playerChunkZ,
							   int renderRadius)
{
	m_radius = renderRadius;
	bool changed = false;

	// 1. Charger/générer les chunks dans le rayon
	for(int cx = playerChunkX - renderRadius;
		cx <= playerChunkX + renderRadius; ++cx)
		{
			for(int cz = playerChunkZ - renderRadius;
				cz <= playerChunkZ + renderRadius; ++cz)
				{
					glm::ivec2 key(cx, cz);
					if(m_chunks.find(key) != m_chunks.end())
						continue; // Déjà chargé

					auto chunk = std::make_unique<Chunk>(cx, cz);

					// Essayer de charger depuis le disque
					if(!m_worldDir.empty()
					   && ChunkSerializer::load(*chunk, m_worldDir))
						{
							LOG_DEBUG("Loaded chunk " << cx << ", " << cz << " from disk.");
						}
					else
						{
							LOG_DEBUG("Generating chunk " << cx << ", " << cz << "...");
							generateTerrain(*chunk);

							// Sauvegarder sur disque
							if(!m_worldDir.empty())
								ChunkSerializer::save(*chunk, m_worldDir);
						}

					// Mark new chunk and neighbors as dirty for meshing
					chunk->setDirty();
					if(Chunk *n = getChunk(cx + 1, cz))
						n->setDirty();
					if(Chunk *n = getChunk(cx - 1, cz))
						n->setDirty();
					if(Chunk *n = getChunk(cx, cz + 1))
						n->setDirty();
					if(Chunk *n = getChunk(cx, cz - 1))
						n->setDirty();

					m_chunks[key] = std::move(chunk);
					changed = true;
				}
		}

	// 2. Décharger les chunks hors du rayon (avec marge +2 pour éviter le
	// flickering)
	int unloadRadius = renderRadius + 2;
	std::vector<glm::ivec2> toRemove;
	for(auto &[pos, chunk] : m_chunks)
		{
			if(std::abs(pos.x - playerChunkX) > unloadRadius
			   || std::abs(pos.y - playerChunkZ) > unloadRadius)
				{
					// Sauvegarder avant de décharger
					if(!m_worldDir.empty())
						ChunkSerializer::save(*chunk, m_worldDir);
					LOG_DEBUG("Unloading chunk " << pos.x << ", " << pos.y);
					toRemove.push_back(pos);
				}
		}

	for(auto &pos : toRemove)
		{
			m_chunks.erase(pos);
			m_chunkMeshes.erase(pos);
			changed = true;
		}

	return changed;
}

// ── Build World Mesh ────────────────────────────────────────────
void World::buildWorldMesh(std::vector<Vertex> &outVertices,
						   std::vector<uint32_t> &outIndices)
{
	outVertices.clear();
	outIndices.clear();

	for(auto &[pos, chunk] : m_chunks)
		{
			// Generate if dirty
			if(chunk->isDirty())
				{
					m_chunkMeshes[pos] = generateChunkMesh(*chunk, this);
					chunk->clearDirty();
				}

			// Offset in world coordinates
			float offsetX = static_cast<float>(pos.x * CHUNK_SIZE_X);
			float offsetZ = static_cast<float>(pos.y * CHUNK_SIZE_Z);

			// Append cached chunk mesh
			const ChunkMesh &mesh = m_chunkMeshes[pos];
			uint32_t baseIndex = static_cast<uint32_t>(outVertices.size());

			for(const auto &v : mesh.vertices)
				{
					Vertex wv = v;
					wv.pos.x += offsetX;
					wv.pos.z += offsetZ;
					outVertices.push_back(wv);
				}
			for(auto idx : mesh.indices)
				{
					outIndices.push_back(baseIndex + idx);
				}
		}

	LOG_INFO("World mesh updated: " << outVertices.size()
			 << " vertices, " << outIndices.size() << " indices ("
			 << m_chunks.size() << " chunks)");
}
