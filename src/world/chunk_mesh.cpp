// -----------------------------------------------------------------------------
// Fichier : chunk_mesh.cpp
// Rôle : Maillage d'un chunk : culling des faces cachées, occlusion ambiante
// par sommet (avec inversion de diagonale), plantes en croix, eau abaissée,
// transitions de surface (herbe / sable / neige) sur les dessus et les marches.
// -----------------------------------------------------------------------------
#include "chunk_mesh.h"

#include "block.h"

#include <algorithm>
#include <deque>

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

		// Surfaces qui se fondent l'une dans l'autre (herbe / sable / neige)
		uint32_t blendKindOf(BlockType t)
		{
			switch(t)
				{
				case BlockType::Grass: return 1;
				case BlockType::Sand: return 2;
				case BlockType::Snow: return 3;
				default: return 0;
				}
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

		// ── Lumière des blocs (lanternes, torches) ──────────────
		// Propagation par niveaux (15 -> 0, -1 par bloc traversé) dans une
		// grille couvrant le chunk et sa portée, à partir des sources du
		// voisinage 3x3. Calculée seulement s'il y a des sources proches.
		struct LightGrid
		{
			static constexpr int R = 15;               // portée maximale
			static constexpr int S = CHUNK_SIZE_X + 2 * R; // x, z dans [-R, 16 + R)
			int y0 = 0, y1 = -1;
			std::vector<uint8_t> v;

			bool empty() const { return y1 < y0; }
			int index(int x, int y, int z) const
			{
				return ((y - y0) * S + (z + R)) * S + (x + R);
			}
			bool inside(int x, int y, int z) const
			{
				return x >= -R && x < CHUNK_SIZE_X + R && z >= -R && z < CHUNK_SIZE_Z + R && y >= y0
					   && y <= y1;
			}
			uint8_t get(int x, int y, int z) const
			{
				return !empty() && inside(x, y, z) ? v[index(x, y, z)] : 0;
			}
		};

		void computeLights(const ChunkNeighborhood &n, LightGrid &g)
		{
			struct Node
			{
				int x, y, z;
			};
			std::vector<std::pair<Node, uint8_t>> sources;
			int minY = CHUNK_SIZE_Y, maxY = -1;
			for(int i = 0; i < 9; ++i)
				{
					const Chunk *c = n.chunks[i].get();
					if(!c)
						continue;
					int ox = (i % 3 - 1) * CHUNK_SIZE_X, oz = (i / 3 - 1) * CHUNK_SIZE_Z;
					for(uint32_t l : c->getLights())
						{
							Node p{ox + static_cast<int>(l & 15u), static_cast<int>(l >> 8),
								   oz + static_cast<int>((l >> 4) & 15u)};
							if(p.x < -LightGrid::R + 1 || p.x >= CHUNK_SIZE_X + LightGrid::R - 1
							   || p.z < -LightGrid::R + 1 || p.z >= CHUNK_SIZE_Z + LightGrid::R - 1)
								continue;
							uint8_t level = BlockDatabase::LightLevel(n.get(p.x, p.y, p.z));
							if(level == 0)
								continue;
							sources.push_back({p, level});
							minY = std::min(minY, p.y);
							maxY = std::max(maxY, p.y);
						}
				}
			if(sources.empty())
				return;
			g.y0 = std::max(0, minY - LightGrid::R);
			g.y1 = std::min(CHUNK_SIZE_Y - 1, maxY + LightGrid::R);
			g.v.assign(static_cast<size_t>(g.y1 - g.y0 + 1) * LightGrid::S * LightGrid::S, 0);

			std::deque<Node> queue;
			for(auto &[p, level] : sources)
				{
					uint8_t &cell = g.v[g.index(p.x, p.y, p.z)];
					if(level > cell)
						{
							cell = level;
							queue.push_back(p);
						}
				}
			static const int DIRS[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
			while(!queue.empty())
				{
					Node p = queue.front();
					queue.pop_front();
					uint8_t level = g.v[g.index(p.x, p.y, p.z)];
					if(level <= 1)
						continue;
					for(const auto &d : DIRS)
						{
							Node q{p.x + d[0], p.y + d[1], p.z + d[2]};
							if(!g.inside(q.x, q.y, q.z))
								continue;
							uint8_t &cell = g.v[g.index(q.x, q.y, q.z)];
							if(cell >= level - 1 || BlockDatabase::IsOpaqueCube(n.get(q.x, q.y, q.z)))
								continue;
							cell = static_cast<uint8_t>(level - 1);
							queue.push_back(q);
						}
				}
		}

		// Niveau de lumière (0..15) -> canal alpha de la teinte du sommet
		uint32_t withLight(uint32_t tint, uint32_t level)
		{
			return (tint & 0x00FFFFFFu) | (std::min(level * 17u, 255u) << 24);
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
	LightGrid light;
	computeLights(n, light);

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
									float up = 0.8f + ((h >> 8) & 7u) / 7.0f * 0.3f;
									const float a = 0.15f, b = 0.85f;
									glm::vec3 base(x + ox, y, z + oz);
									glm::vec2 d[2][2] = {{{a, a}, {b, b}}, {{a, b}, {b, a}}};
									uint32_t layer = static_cast<uint32_t>(bd.texLayerFront);
									uint32_t plantTint = withLight(tint, std::max<uint32_t>(light.get(x, y, z), bd.light));
									if(!bd.waving)
										{
											// Torches, toiles : ni décalage ni variation de taille
											base = glm::vec3(x, y, z);
											up = 1.0f;
										}
									for(auto &diag : d)
										{
											Vertex v[4];
											glm::vec3 p0 = base + glm::vec3(diag[0].x, 0, diag[0].y);
											glm::vec3 p1 = base + glm::vec3(diag[1].x, 0, diag[1].y);
											glm::vec3 top(0, up, 0);
											uint32_t topWave = bd.waving ? 2u : 0u;
											v[0] = {p0, Vertex::pack(layer, 6, 0, 0, 1, typeId, 0), plantTint};
											v[1] = {p1, Vertex::pack(layer, 6, 0, 1, 1, typeId, 0), plantTint};
											v[2] = {p1 + top, Vertex::pack(layer, 6, 0, 1, 0, typeId, topWave), plantTint};
											v[3] = {p0 + top, Vertex::pack(layer, 6, 0, 0, 0, typeId, topWave), plantTint};
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
											// "Better Grass" : les flancs d'herbe reprennent la
											// texture du dessus (pentes vertes, pas de rayures
											// de terre à chaque marche) ; dessous : terre
											if(f != 3)
												layer = static_cast<uint32_t>(bd.texLayerTop);
											else
												faceTint = TINT_NEUTRAL;
										}
									Vertex v[4];
									uint32_t ao[4] = {0, 0, 0, 0};

									// ── Transition de surface ──
									// Le type voisin (herbe/sable/neige) dont la face supérieure
									// touche celle-ci : le shader mélange les deux textures.
									// Dessus : voisins au même niveau ou à une marche près.
									// Flancs d'une marche : la face descend vers le dessus
									// du voisin en contrebas (dégradé vertical continu).
									uint32_t blendKind = 0;
									BlockType partner = BlockType::Air;
									// Surface de type t dont le dessus est exposé, au niveau
									// ly ou à une marche près (bords de plage en escalier)
									auto topSurface = [&](int bx, int ly, int bz, BlockType t) {
										for(int dy = 0; dy >= -1; --dy)
											if(n.get(bx, ly + dy, bz) == t
											   && !BlockDatabase::IsOpaqueCube(n.get(bx, ly + dy + 1, bz)))
												return true;
										return n.get(bx, ly + 1, bz) == t
											   && !BlockDatabase::IsOpaqueCube(n.get(bx, ly + 2, bz));
									};
									// Part (en quarts) du type t autour du coin supérieur
									// (cx, cz) d'une surface au niveau ly
									auto cornerCount = [&](int bx, int ly, int bz, int cx, int cz,
														   BlockType t) {
										int sx = cx ? 1 : -1, sz = cz ? 1 : -1;
										return static_cast<uint32_t>(topSurface(bx + sx, ly, bz, t))
											   + topSurface(bx, ly, bz + sz, t)
											   + topSurface(bx + sx, ly, bz + sz, t);
									};
									bool sideFace = f != 2 && f != 3;
									bool topExposed = !BlockDatabase::IsOpaqueCube(n.get(x, y + 1, z));
									if(f == 2 && blendKindOf(type) != 0)
										{
											const BlockType candidates[3] = {
											  type == BlockType::Grass ? BlockType::Sand : BlockType::Grass,
											  type == BlockType::Snow ? BlockType::Sand : BlockType::Snow,
											  BlockType::Air};
											for(BlockType cand : candidates)
												{
													if(cand == BlockType::Air || cand == type)
														continue;
													bool found = false;
													for(int dz = -1; dz <= 1 && !found; ++dz)
														for(int dx = -1; dx <= 1 && !found; ++dx)
															if((dx || dz) && topSurface(x + dx, y, z + dz, cand))
																found = true;
													if(found)
														{
															partner = cand;
															blendKind = blendKindOf(cand);
															break;
														}
												}
										}
									else if(sideFace && topExposed && blendKindOf(type) != 0)
										{
											// Marche d'une hauteur : dessus du voisin juste en dessous
											BlockType below = n.get(x + fd.normal.x, y - 1, z + fd.normal.z);
											if(blendKindOf(below) != 0 && below != type)
												{
													partner = below;
													blendKind = blendKindOf(below);
												}
										}
									// L'herbe teinte la texture d'herbe, quelle que soit la face
									if(partner == BlockType::Grass)
										faceTint = grassTint;

									for(int c = 0; c < 4; ++c)
										{
											glm::ivec3 corner = fd.corners[c];
											glm::vec3 p(x + corner.x, y + corner.y, z + corner.z);
											if(isLiquid && corner.y == 1 && !waterAbove)
												p.y = y + WATER_SURFACE;

											uint32_t lightLevel = bd.light;
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

													// Lumière des blocs : moyenne des cellules non opaques
													// autour du coin (éclairage lissé)
													if(!light.empty() && lightLevel == 0)
														{
															auto open = [&](const glm::ivec3 &q) {
																return !BlockDatabase::IsOpaqueCube(n.get(q.x, q.y, q.z));
															};
															glm::ivec3 q1 = base + d1, q2 = base + d2, q3 = base + d1 + d2;
															uint32_t sum = light.get(base.x, base.y, base.z), cnt = 1;
															bool o1 = open(q1), o2 = open(q2);
															if(o1)
																{
																	sum += light.get(q1.x, q1.y, q1.z);
																	cnt++;
																}
															if(o2)
																{
																	sum += light.get(q2.x, q2.y, q2.z);
																	cnt++;
																}
															if((o1 || o2) && open(q3))
																{
																	sum += light.get(q3.x, q3.y, q3.z);
																	cnt++;
																}
															lightLevel = (sum + cnt / 2) / cnt;
														}
												}

											uint32_t blendCount = 0;
											if(blendKind != 0 && (!sideFace || corner.y == 1))
												{
													// Dessus, ou arête haute d'un flanc : même valeur
													// que le coin du dessus (continuité sur l'arête)
													blendCount = cornerCount(x, y, z, corner.x, corner.z, partner);
												}
											else if(blendKind != 0)
												{
													// Arête basse d'un flanc : complément de la part de
													// ce type vue depuis le dessus du voisin en contrebas
													int px = x + fd.normal.x, pz = z + fd.normal.z;
													int cx = x + corner.x - px, cz = z + corner.z - pz;
													blendCount = 4u - cornerCount(px, y - 1, pz, cx, cz, type);
												}
											v[c] = {p, Vertex::pack(layer, static_cast<uint32_t>(f), ao[c],
																	FACE_U[c], FACE_V[c], typeId, wave,
																	blendKind, blendCount),
													withLight(faceTint, lightLevel)};
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
