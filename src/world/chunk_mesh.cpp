// -----------------------------------------------------------------------------
// Fichier : chunk_mesh.cpp
// Rôle : Maillage d'un chunk : culling des faces cachées, occlusion ambiante
// par sommet (avec inversion de diagonale), plantes en croix, eau abaissée.
// -----------------------------------------------------------------------------
#include "chunk_mesh.h"

#include "block.h"

#include <algorithm>

namespace
	{
		// Coins de chaque face (ordre CCW vu de l'extérieur) et UV associés
		struct FaceDef
		{
			glm::ivec3 normal;
			glm::ivec3 corners[4];
		};

		// 0 +Z, 1 -Z, 2 +Y, 3 -Y, 4 +X, 5 -X
		const FaceDef FACES[6] = {
		  {{0, 0, 1}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}},
		  {{0, 0, -1}, {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}},
		  {{0, 1, 0}, {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}},
		  {{0, -1, 0}, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}},
		  {{1, 0, 0}, {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}},
		  {{-1, 0, 0}, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}},
		};
		const uint32_t FACE_U[4] = {0, 1, 1, 0};
		const uint32_t FACE_V[4] = {1, 1, 0, 0};

		constexpr float WATER_SURFACE = 0.875f;

		int faceLayer(const BlockData &bd, int face)
		{
			switch(face)
				{
				case 0: return bd.texLayerFront;
				case 1: return bd.texLayerBack;
				case 2: return bd.texLayerTop;
				case 3: return bd.texLayerBottom;
				case 4: return bd.texLayerRight;
				default: return bd.texLayerLeft;
				}
		}

		// Bloc qui projette de l'occlusion ambiante
		bool occludes(BlockType t)
		{
			const BlockData &d = BlockDatabase::Get(t);
			return t != BlockType::Air && d.renderType == RenderType::Cube;
		}

		uint32_t hashPos(int x, int y, int z)
		{
			uint32_t h = static_cast<uint32_t>(x) * 73856093u
						 ^ static_cast<uint32_t>(y) * 19349663u
						 ^ static_cast<uint32_t>(z) * 83492791u;
			h ^= h >> 13;
			h *= 0x5bd1e995u;
			h ^= h >> 15;
			return h;
		}

		struct MeshBuilder
		{
			std::vector<Vertex> vertices;
			std::vector<uint32_t> indices;

			void quad(const Vertex v[4], bool flip)
			{
				uint32_t base = static_cast<uint32_t>(vertices.size());
				vertices.insert(vertices.end(), v, v + 4);
				if(!flip)
					indices.insert(indices.end(), {base, base + 1, base + 2,
												   base + 2, base + 3, base});
				else
					indices.insert(indices.end(), {base + 1, base + 2, base + 3,
												   base + 3, base, base + 1});
			}
		};
	}

ChunkMeshData buildChunkMesh(const ChunkNeighborhood &n)
{
	const Chunk &chunk = n.center();
	MeshBuilder solid;
	MeshBuilder cutout;
	MeshBuilder water;
	glm::ivec2 cpos = chunk.getChunkPos();
	int topY = std::min(chunk.getTopY(), CHUNK_SIZE_Y);

	for(int z = 0; z < CHUNK_SIZE_Z; ++z)
		{
			for(int x = 0; x < CHUNK_SIZE_X; ++x)
				{
					uint32_t grassTint = chunk.getGrassTint(x, z);
					uint32_t foliageTint = chunk.getFoliageTint(x, z);

					for(int y = 0; y < topY; ++y)
						{
							BlockType type = chunk.getBlockUnchecked(x, y, z);
							if(type == BlockType::Air)
								continue;

							const BlockData &bd = BlockDatabase::Get(type);
							uint32_t typeId = static_cast<uint32_t>(type);

							uint32_t tint = TINT_NEUTRAL;
							if(bd.tint == TintType::Foliage)
								tint = foliageTint;
							else if(bd.tint == TintType::Grass)
								tint = grassTint;

							// ── Plantes : deux quads en croix, double face ──
							if(bd.renderType == RenderType::Cross)
								{
									uint32_t h = hashPos(cpos.x * 16 + x, y,
														 cpos.y * 16 + z);
									float ox = ((h & 15u) / 15.0f - 0.5f) * 0.4f;
									float oz = (((h >> 4) & 15u) / 15.0f - 0.5f) * 0.4f;
									float sy = 0.8f + ((h >> 8) & 7u) / 7.0f * 0.3f;
									const float a = 0.15f, b = 0.85f;
									glm::vec3 base(x + ox, y, z + oz);
									glm::vec2 d[2][2] = {{{a, a}, {b, b}}, {{a, b}, {b, a}}};
									uint32_t layer = static_cast<uint32_t>(bd.texLayerFront);
									for(auto &diag : d)
										{
											Vertex v[4];
											glm::vec3 p0 = base + glm::vec3(diag[0].x, 0, diag[0].y);
											glm::vec3 p1 = base + glm::vec3(diag[1].x, 0, diag[1].y);
											glm::vec3 up(0, sy, 0);
											v[0] = {p0, Vertex::pack(layer, 6, 0, 0, 1, typeId, 0), tint};
											v[1] = {p1, Vertex::pack(layer, 6, 0, 1, 1, typeId, 0), tint};
											v[2] = {p1 + up, Vertex::pack(layer, 6, 0, 1, 0, typeId, 2), tint};
											v[3] = {p0 + up, Vertex::pack(layer, 6, 0, 0, 0, typeId, 2), tint};
											cutout.quad(v, false);
											Vertex r[4] = {v[1], v[0], v[3], v[2]};
											cutout.quad(r, false);
										}
									continue;
								}

							bool isLiquid = bd.renderType == RenderType::Liquid;
							// Cubes transparents (feuillages, vitres) : découpe alpha
							MeshBuilder &target = isLiquid ? water : (bd.isTransparent ? cutout : solid);
							bool waterAbove = isLiquid && n.get(x, y + 1, z) == BlockType::Water;
							uint32_t wave = bd.waving ? 1u : 0u;

							for(int f = 0; f < 6; ++f)
								{
									const FaceDef &fd = FACES[f];
									BlockType nb = n.get(x + fd.normal.x, y + fd.normal.y,
														 z + fd.normal.z);

									// ── Visibilité de la face ──
									bool visible;
									if(isLiquid)
										visible = nb != BlockType::Water
												  && !BlockDatabase::IsOpaqueCube(nb);
									else if(BlockDatabase::IsOpaqueCube(nb))
										visible = false;
									else if(nb == type && bd.isTransparent
											&& type == BlockType::Glass)
										visible = false; // vitres jointives
									else
										visible = true;
									if(!visible)
										continue;

									// Teinte : herbe uniquement sur le dessus
									uint32_t faceTint = tint;
									uint32_t layer = static_cast<uint32_t>(faceLayer(bd, f));
									if(type == BlockType::Grass && f != 2)
										{
											faceTint = TINT_NEUTRAL;
											// "Better Grass" : flanc d'une marche qui descend vers
											// une autre herbe -> texture du dessus (pentes vertes)
											if(f != 3
											   && n.get(x + fd.normal.x, y - 1, z + fd.normal.z)
													== BlockType::Grass)
												{
													layer = static_cast<uint32_t>(bd.texLayerTop);
													faceTint = tint;
												}
										}
									Vertex v[4];
									uint32_t ao[4] = {0, 0, 0, 0};

									for(int c = 0; c < 4; ++c)
										{
											glm::ivec3 corner = fd.corners[c];
											glm::vec3 p(x + corner.x, y + corner.y, z + corner.z);
											if(isLiquid && corner.y == 1 && !waterAbove)
												p.y = y + WATER_SURFACE;

											if(!isLiquid)
												{
													// Occlusion ambiante du coin
													glm::ivec3 base = glm::ivec3(x, y, z) + fd.normal;
													glm::ivec3 d1(0), d2(0);
													if(fd.normal.x != 0)
														{
															d1.y = corner.y ? 1 : -1;
															d2.z = corner.z ? 1 : -1;
														}
													else if(fd.normal.y != 0)
														{
															d1.x = corner.x ? 1 : -1;
															d2.z = corner.z ? 1 : -1;
														}
													else
														{
															d1.x = corner.x ? 1 : -1;
															d2.y = corner.y ? 1 : -1;
														}
													bool s1 = occludes(n.get(base.x + d1.x, base.y + d1.y, base.z + d1.z));
													bool s2 = occludes(n.get(base.x + d2.x, base.y + d2.y, base.z + d2.z));
													bool cc = occludes(n.get(base.x + d1.x + d2.x,
																			 base.y + d1.y + d2.y,
																			 base.z + d1.z + d2.z));
													ao[c] = (s1 && s2) ? 3u : static_cast<uint32_t>(s1 + s2 + cc);
												}

											v[c] = {p, Vertex::pack(layer, static_cast<uint32_t>(f), ao[c],
																	FACE_U[c], FACE_V[c], typeId, wave),
													faceTint};
										}

									// Diagonale qui évite l'anisotropie de l'AO
									bool flip = ao[0] + ao[2] > ao[1] + ao[3];
									target.quad(v, flip);
								}
						}
				}
		}

	ChunkMeshData mesh;
	mesh.chunkPos = cpos;
	mesh.opaqueIndexCount = static_cast<uint32_t>(solid.indices.size());
	mesh.cutoutIndexCount = static_cast<uint32_t>(cutout.indices.size());
	mesh.waterIndexCount = static_cast<uint32_t>(water.indices.size());

	// Concatène sommets et indices : opaque, cutout, eau
	mesh.vertices = std::move(solid.vertices);
	mesh.indices = std::move(solid.indices);
	for(MeshBuilder *b : {&cutout, &water})
		{
			uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
			mesh.vertices.insert(mesh.vertices.end(), b->vertices.begin(), b->vertices.end());
			for(uint32_t i : b->indices)
				mesh.indices.push_back(i + base);
		}

	if(!mesh.vertices.empty())
		{
			mesh.minY = mesh.maxY = mesh.vertices[0].pos.y;
			for(const Vertex &v : mesh.vertices)
				{
					mesh.minY = std::min(mesh.minY, v.pos.y);
					mesh.maxY = std::max(mesh.maxY, v.pos.y);
				}
		}
	return mesh;
}
