#include "world.h"

#include "block.h"
#include "chunk_serializer.h"
#include "core/logger.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

// ── Construction ────────────────────────────────────────────────
World::World(uint32_t seed) : m_generator(seed)
{
	unsigned hw = std::thread::hardware_concurrency();
	unsigned workers = hw > 3 ? hw - 2 : 2;
	m_jobs = std::make_unique<JobSystem>(workers);
	LOG_INFO("World: " << workers << " worker threads for generation/meshing");
}

World::~World()
{
	// Arrête les threads avant de détruire les chunks qu'ils manipulent
	m_jobs.reset();
}

void World::setWorldDir(const std::string &dir)
{
	m_worldDir = dir;
	std::filesystem::create_directories(m_worldDir);
}

void World::setRenderRadius(int radius)
{
	radius = std::clamp(radius, 2, 48);
	if(radius == m_radius)
		return;
	m_radius = radius;
	m_atomicRadius = radius;
	if(m_centerValid)
		{
			unloadFar();
			scheduleAround();
		}
}

bool World::inRadius(const glm::ivec2 &pos, float radius) const
{
	float dx = static_cast<float>(pos.x - m_center.x);
	float dz = static_cast<float>(pos.y - m_center.y);
	return dx * dx + dz * dz <= radius * radius;
}

// ── Chunk Access ────────────────────────────────────────────────
const Chunk *World::getChunk(int cx, int cz) const
{
	auto it = m_chunks.find(glm::ivec2(cx, cz));
	return it != m_chunks.end() ? it->second.get() : nullptr;
}

BlockType World::getBlockAt(int wx, int wy, int wz) const
{
	if(wy < 0 || wy >= CHUNK_SIZE_Y)
		return BlockType::Air;
	int cx = floorDiv(wx, CHUNK_SIZE_X);
	int cz = floorDiv(wz, CHUNK_SIZE_Z);
	const Chunk *chunk = getChunk(cx, cz);
	if(!chunk)
		return BlockType::Air;
	return chunk->getBlockUnchecked(wx - cx * CHUNK_SIZE_X, wy,
									wz - cz * CHUNK_SIZE_Z);
}

bool World::isColumnLoaded(int wx, int wz) const
{
	return getChunk(floorDiv(wx, CHUNK_SIZE_X), floorDiv(wz, CHUNK_SIZE_Z))
		   != nullptr;
}

// ── Update (thread principal) ───────────────────────────────────
void World::update(const glm::vec3 &playerPos)
{
	glm::ivec2 pc(floorDiv(static_cast<int>(std::floor(playerPos.x)), CHUNK_SIZE_X),
				  floorDiv(static_cast<int>(std::floor(playerPos.z)), CHUNK_SIZE_Z));
	if(!m_centerValid || pc != m_center)
		{
			m_center = pc;
			m_centerValid = true;
			m_atomicCX = pc.x;
			m_atomicCZ = pc.y;
			unloadFar();
			scheduleAround();
		}
	drainResults();
}

// Planifie la génération des chunks manquants (rayon de rendu + 1, pour que
// les chunks du bord aient leurs voisins avant d'être maillés)
void World::scheduleAround()
{
	int genR = m_radius + 1;
	std::vector<std::pair<float, glm::ivec2>> todo;
	for(int dx = -genR; dx <= genR; ++dx)
		for(int dz = -genR; dz <= genR; ++dz)
			{
				glm::ivec2 pos = m_center + glm::ivec2(dx, dz);
				float d2 = static_cast<float>(dx * dx + dz * dz);
				if(d2 > (genR + 0.5f) * (genR + 0.5f))
					continue;
				if(m_chunks.count(pos) || m_genPending.count(pos))
					continue;
				todo.push_back({std::sqrt(d2), pos});
			}
	std::sort(todo.begin(), todo.end(),
			  [](const auto &a, const auto &b) { return a.first < b.first; });

	for(auto &[dist, pos] : todo)
		{
			m_genPending.insert(pos);
			m_jobs->submit(dist, [this, pos] {
				// Annulé si le joueur s'est éloigné entre-temps
				int dx = pos.x - m_atomicCX.load();
				int dz = pos.y - m_atomicCZ.load();
				float r = static_cast<float>(m_atomicRadius.load()) + 2.5f;
				if(static_cast<float>(dx * dx + dz * dz) > r * r)
					{
						std::lock_guard<std::mutex> lock(m_resultMutex);
						m_genCancelled.push_back(pos);
						return;
					}

				auto chunk = std::make_shared<Chunk>(pos.x, pos.y);
				if(!m_worldDir.empty() && ChunkSerializer::load(*chunk, m_worldDir))
					m_generator.computeTints(*chunk);
				else
					m_generator.generate(*chunk);

				std::lock_guard<std::mutex> lock(m_resultMutex);
				m_genResults.push_back(std::move(chunk));
			});
		}
}

void World::unloadFar()
{
	float unloadR = m_radius + 2.5f;
	std::vector<glm::ivec2> toRemove;
	for(auto &[pos, chunk] : m_chunks)
		if(!inRadius(pos, unloadR))
			toRemove.push_back(pos);

	for(auto &pos : toRemove)
		{
			auto it = m_chunks.find(pos);
			if(it->second->isModified() && !m_worldDir.empty())
				ChunkSerializer::save(*it->second, m_worldDir);
			m_chunks.erase(it);

			auto ms = m_meshState.find(pos);
			if(ms != m_meshState.end())
				{
					if(ms->second == MeshState::Done)
						{
							m_events.push_back({pos, nullptr});
							m_meshedCount--;
						}
					m_meshState.erase(ms);
				}
		}
}

void World::tryScheduleMesh(const glm::ivec2 &pos)
{
	auto it = m_chunks.find(pos);
	if(it == m_chunks.end())
		return;
	auto ms = m_meshState.find(pos);
	if(ms != m_meshState.end())
		return; // déjà maillé ou en cours
	if(!inRadius(pos, m_radius + 0.5f))
		return;

	ChunkNeighborhood n;
	for(int dz = -1; dz <= 1; ++dz)
		for(int dx = -1; dx <= 1; ++dx)
			{
				auto nit = m_chunks.find(pos + glm::ivec2(dx, dz));
				if(nit == m_chunks.end())
					return; // un voisin manque encore
				n.chunks[(dx + 1) + (dz + 1) * 3] = nit->second;
			}

	m_meshState[pos] = MeshState::Pending;
	float dist = std::sqrt(static_cast<float>((pos.x - m_center.x) * (pos.x - m_center.x)
											  + (pos.y - m_center.y) * (pos.y - m_center.y)));
	m_jobs->submit(dist - 0.5f, [this, n = std::move(n)] {
		auto mesh = std::make_shared<ChunkMeshData>(buildChunkMesh(n));
		std::lock_guard<std::mutex> lock(m_resultMutex);
		m_meshResults.push_back(std::move(mesh));
	});
}

void World::drainResults()
{
	std::vector<std::shared_ptr<Chunk>> generated;
	std::vector<glm::ivec2> cancelled;
	std::vector<std::shared_ptr<ChunkMeshData>> meshes;
	{
		std::lock_guard<std::mutex> lock(m_resultMutex);
		generated.swap(m_genResults);
		cancelled.swap(m_genCancelled);
		meshes.swap(m_meshResults);
	}

	for(auto &pos : cancelled)
		m_genPending.erase(pos);

	std::vector<glm::ivec2> added;
	for(auto &chunk : generated)
		{
			glm::ivec2 pos = chunk->getChunkPos();
			m_genPending.erase(pos);
			if(!inRadius(pos, m_radius + 2.5f))
				continue; // trop loin désormais
			m_chunks[pos] = std::move(chunk);
			added.push_back(pos);
		}

	// Les chunks ajoutés et leurs voisins peuvent devenir maillables
	for(const glm::ivec2 &pos : added)
		for(int dz = -1; dz <= 1; ++dz)
			for(int dx = -1; dx <= 1; ++dx)
				tryScheduleMesh(pos + glm::ivec2(dx, dz));

	for(auto &mesh : meshes)
		{
			auto ms = m_meshState.find(mesh->chunkPos);
			if(ms == m_meshState.end() || ms->second != MeshState::Pending
			   || !m_chunks.count(mesh->chunkPos))
				continue;
			ms->second = MeshState::Done;
			m_meshedCount++;
			m_events.push_back({mesh->chunkPos, std::move(mesh)});
		}
}

std::vector<World::MeshEvent> World::takeMeshEvents(size_t maxUploads)
{
	std::vector<MeshEvent> out;
	size_t uploads = 0;
	while(!m_events.empty())
		{
			if(m_events.front().mesh)
				{
					if(uploads >= maxUploads)
						break;
					uploads++;
				}
			out.push_back(std::move(m_events.front()));
			m_events.pop_front();
		}
	return out;
}

void World::saveAll()
{
	if(m_worldDir.empty())
		return;
	for(auto &[pos, chunk] : m_chunks)
		if(chunk->isModified())
			ChunkSerializer::save(*chunk, m_worldDir);
}
