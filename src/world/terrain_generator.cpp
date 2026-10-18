// -----------------------------------------------------------------------------
// Fichier : terrain_generator.cpp
// Rôle : Implémentation de la génération procédurale du terrain.
// -----------------------------------------------------------------------------
#include "terrain_generator.h"

#include <algorithm>
#include <cmath>

namespace
	{
		float smoothstep(float e0, float e1, float x)
		{
			float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float mix(float a, float b, float t) { return a + (b - a) * t; }

		// Teinte multiplicative encodée sur [0, 2] (voir Vertex::tint)
		uint32_t packColor(float r, float g, float b)
		{
			auto c = [](float v) {
				return static_cast<uint32_t>(std::clamp(v * 0.5f, 0.0f, 1.0f) * 255.0f
											 + 0.5f);
			};
			return c(r) | (c(g) << 8) | (c(b) << 16) | (255u << 24);
		}

		bool isPlant(BlockType t)
		{
			return BlockDatabase::Get(t).renderType == RenderType::Cross;
		}

		// Taille de la grille de placement des arbres (un candidat par cellule)
		constexpr int TREE_CELL = 5;
		// Rayon horizontal maximal d'un arbre autour de son tronc
		constexpr int TREE_REACH = 4;
		// Au-delà, plus d'arbres (limite de la forêt)
		constexpr int TREE_LINE = 150;
	}

// ── Constructeur ────────────────────────────────────────────────
TerrainGenerator::TerrainGenerator(uint32_t seed, bool amplified)
	: m_seed(seed), m_continentNoise(seed), m_erosionNoise(seed + 1),
	  m_ridgeNoise(seed + 2), m_detailNoise(seed + 3), m_hillNoise(seed + 4),
	  m_lakeNoise(seed + 5), m_tempNoise(seed + 100),
	  m_humidityNoise(seed + 200), m_cherryNoise(seed + 300),
	  m_surfaceNoise(seed + 400), m_amplifiedNoise(seed + 500),
	  m_overhangNoise(seed + 600), m_amplifiedWorld(amplified),
	  m_structures(std::make_unique<StructureGenerator>(*this))
{}

// ── Relief amplifié ─────────────────────────────────────────────
float TerrainGenerator::amplifiedMask(int wx, int wz) const
{
	if(m_amplifiedWorld)
		return 1.0f;
	float n = m_amplifiedNoise.octaveNoise(wx * 0.0011f, wz * 0.0011f, 3, 2.0f, 0.5f);
	return smoothstep(0.20f, 0.32f, n);
}

float TerrainGenerator::overhangStrength(int wx, int wz, int slope) const
{
	// Seulement sur les pentes : vallées et plateaux restent praticables
	return 1.25f * amplifiedMask(wx, wz) * smoothstep(1.5f, 5.0f, static_cast<float>(slope));
}

float TerrainGenerator::overhangDensity(int wx, int y, int wz, int height, float strength) const
{
	float d = (height - 0.5f - y) / 20.0f;
	if(strength <= 0.0f)
		return d;
	float n = m_overhangNoise.noise3(wx * 0.024f, y * 0.032f, wz * 0.024f)
			  + 0.4f * m_overhangNoise.noise3(wx * 0.061f + 19.3f, y * 0.075f, wz * 0.061f - 7.1f);
	return d + n * strength;
}

// ── Hash ────────────────────────────────────────────────────────
uint32_t TerrainGenerator::hash(int x, int z, uint32_t salt) const
{
	uint64_t h = static_cast<uint64_t>(static_cast<uint32_t>(x)) * 0x9E3779B97F4A7C15ull;
	h ^= static_cast<uint64_t>(static_cast<uint32_t>(z)) * 0xC2B2AE3D27D4EB4Full;
	h ^= (static_cast<uint64_t>(salt) << 32) ^ m_seed;
	// splitmix64 finalizer
	h ^= h >> 30;
	h *= 0xBF58476D1CE4E5B9ull;
	h ^= h >> 27;
	h *= 0x94D049BB133111EBull;
	h ^= h >> 31;
	return static_cast<uint32_t>(h);
}

float TerrainGenerator::hash01(int x, int z, uint32_t salt) const
{
	return static_cast<float>(hash(x, z, salt) >> 8) / 16777216.0f;
}

// ── Climat ──────────────────────────────────────────────────────
float TerrainGenerator::getTemperature(int wx, int wz) const
{
	float x = static_cast<float>(wx), z = static_cast<float>(wz);
	float t = m_tempNoise.octaveNoise(x * 0.0013f, z * 0.0013f, 4, 2.0f, 0.5f);
	t += m_detailNoise.noise(x * 0.011f, z * 0.011f) * 0.05f;
	return std::clamp(0.5f + t * 1.5f, 0.0f, 1.0f);
}

float TerrainGenerator::getHumidity(int wx, int wz) const
{
	float x = static_cast<float>(wx), z = static_cast<float>(wz);
	float h
	  = m_humidityNoise.octaveNoise(x * 0.0017f, z * 0.0017f, 4, 2.0f, 0.5f);
	return std::clamp(0.5f + h * 1.5f, 0.0f, 1.0f);
}

int TerrainGenerator::getSnowLine(int wx, int wz) const
{
	float n = m_surfaceNoise.noise(wx * 0.03f, wz * 0.03f);
	return 156 + static_cast<int>(n * 12.0f);
}

// ── Relief ──────────────────────────────────────────────────────
int TerrainGenerator::getHeightAt(int wx, int wz) const
{
	float x = static_cast<float>(wx), z = static_cast<float>(wz);

	// Continents : grandes masses de terre / océans
	float c
	  = m_continentNoise.octaveNoise(x * 0.0010f, z * 0.0010f, 5, 2.0f, 0.5f);
	// Érosion : faible érosion = montagnes
	float e = m_erosionNoise.octaveNoise(x * 0.0019f, z * 0.0019f, 4, 2.0f, 0.5f);
	float hills = m_hillNoise.octaveNoise(x * 0.0070f, z * 0.0070f, 4, 2.0f, 0.5f);
	float detail = m_detailNoise.octaveNoise(x * 0.025f, z * 0.025f, 3, 2.0f, 0.5f);

	float h = SEA_LEVEL + 9.0f + c * 42.0f;
	h += hills * 10.0f + detail * 2.0f;

	// Océans plus profonds
	h -= smoothstep(-0.2f, -0.5f, c) * 16.0f;

	// Montagnes : masque continental × faible érosion
	float m = smoothstep(-0.05f, 0.28f, c) * smoothstep(0.18f, -0.22f, e);
	if(m > 0.0f)
		{
			// Domain warping : crêtes moins régulières
			float wx = x + hills * 60.0f;
			float wz = z + detail * 25.0f
					   + m_hillNoise.noise(z * 0.007f, x * 0.007f) * 60.0f;
			// Multifractale "ridged" (Musgrave) : arêtes vives et ramifiées
			float sum = 0.0f, amp = 0.55f, freq = 0.0042f, weight = 1.0f;
			for(int i = 0; i < 6; ++i)
				{
					float n = 1.0f - std::abs(m_ridgeNoise.noise(wx * freq, wz * freq));
					n *= n;
					n *= weight;
					weight = std::clamp(n * 1.8f, 0.0f, 1.0f);
					sum += n * amp;
					amp *= 0.5f;
					freq *= 2.05f;
				}
			h += m * (std::pow(sum, 1.35f) * 175.0f + 10.0f);
		}

	// Pics amplifiés : multifractale plus haute et plus abrupte, vallées
	// profondes entre les aiguilles (seulement sur les terres émergées)
	float amp = amplifiedMask(wx, wz) * smoothstep(-0.14f, 0.04f, c);
	if(amp > 0.0f)
		{
			float ax = x + hills * 45.0f + 913.0f, az = z + detail * 30.0f - 377.0f;
			float sum = 0.0f, a = 0.6f, freq = 0.0068f, weight = 1.0f;
			for(int i = 0; i < 5; ++i)
				{
					float n = 1.0f - std::abs(m_ridgeNoise.noise(ax * freq, az * freq));
					n *= n * weight;
					weight = std::clamp(n * 2.0f, 0.0f, 1.0f);
					sum += n * a;
					a *= 0.5f;
					freq *= 2.1f;
				}
			float spires = SEA_LEVEL + 3.0f + std::pow(sum, 1.55f) * 225.0f + hills * 16.0f;
			h = mix(h, std::max(spires, SEA_LEVEL - 6.0f), amp);
		}

	// Lacs : cuvettes au fond des vallées (moins marqués en montagne)
	float l = m_lakeNoise.octaveNoise(x * 0.0050f, z * 0.0050f, 3, 2.0f, 0.5f);
	float lake = smoothstep(0.14f, 0.38f, l) * (1.0f - m * 0.9f) * (1.0f - amp * 0.7f);
	if(lake > 0.0f)
		{
			float bottom = SEA_LEVEL - 3.0f - 7.0f * smoothstep(0.3f, 0.5f, l);
			h = std::min(h, mix(h, bottom, lake));
		}

	// Compression douce des sommets (pas de plateau écrêté sous la limite
	// du monde) : au-delà de 175, la hauteur tend vers 242
	if(h > 175.0f)
		h = 175.0f + 67.0f * (1.0f - std::exp(-(h - 175.0f) / 67.0f));
	return static_cast<int>(std::clamp(std::round(h), 4.0f, 245.0f));
}

// ── Biomes ──────────────────────────────────────────────────────
BiomeType TerrainGenerator::selectBiome(int height, float temp, float humidity,
										int wx, int wz) const
{
	if(height <= SEA_LEVEL)
		return BiomeType::Ocean;
	// Limite plage / herbe irrégulière (pas une simple courbe de niveau)
	int beachTop = SEA_LEVEL + 1
				   + static_cast<int>(std::round(1.4f + 1.4f * m_surfaceNoise.noise(wx * 0.045f, wz * 0.045f)));
	if(height <= beachTop && temp > 0.2f)
		return BiomeType::Beach;
	if(height >= getSnowLine(wx, wz))
		return BiomeType::SnowyPeaks;
	// Monde amplifié : climats variés dans les vallées, pics au-dessus
	if(m_amplifiedWorld ? height > 112 : amplifiedMask(wx, wz) > 0.5f)
		return BiomeType::Amplified;
	if(height > 120)
		return BiomeType::Mountains;

	// L'altitude refroidit le climat
	float t = temp - std::max(0, height - 90) * 0.006f;
	if(t < 0.15f)
		return BiomeType::SnowyTaiga;
	if(t < 0.30f)
		return BiomeType::Taiga;
	if(t > 0.7f && humidity < 0.32f)
		return BiomeType::Desert;

	float cherry = m_cherryNoise.octaveNoise(wx * 0.0040f, wz * 0.0040f, 3,
											 2.0f, 0.5f);
	if(cherry > 0.16f && t > 0.38f && t < 0.72f && humidity > 0.28f)
		return BiomeType::CherryGrove;
	if(humidity > 0.55f)
		return BiomeType::Forest;
	return BiomeType::Plains;
}

ColumnInfo TerrainGenerator::getColumn(int wx, int wz) const
{
	ColumnInfo info;
	info.height = getHeightAt(wx, wz);
	info.temperature = getTemperature(wx, wz);
	info.humidity = getHumidity(wx, wz);
	info.biome
	  = selectBiome(info.height, info.temperature, info.humidity, wx, wz);
	return info;
}

// ── Teintes (multiplicateurs : les textures sont déjà colorées) ─
uint32_t TerrainGenerator::grassTint(float temp, float humidity) const
{
	glm::vec3 dry(1.30f, 1.18f, 0.62f);
	glm::vec3 lush(1.05f, 1.42f, 0.80f);
	glm::vec3 cold(0.92f, 1.15f, 1.05f);
	glm::vec3 c = glm::mix(dry, lush, humidity);
	c = glm::mix(c, cold, std::clamp(1.0f - temp * 2.2f, 0.0f, 1.0f));
	return packColor(c.r, c.g, c.b);
}

uint32_t TerrainGenerator::foliageTint(float temp, float humidity) const
{
	glm::vec3 dry(1.10f, 1.00f, 0.65f);
	glm::vec3 lush(0.86f, 1.04f, 0.80f);
	glm::vec3 cold(0.80f, 0.98f, 0.95f);
	glm::vec3 c = glm::mix(dry, lush, humidity);
	c = glm::mix(c, cold, std::clamp(1.0f - temp * 2.2f, 0.0f, 1.0f));
	return packColor(c.r, c.g, c.b);
}

void TerrainGenerator::computeTints(Chunk &chunk) const
{
	glm::ivec3 origin = chunk.toWorldPos(0, 0, 0);
	for(int z = 0; z < CHUNK_SIZE_Z; ++z)
		for(int x = 0; x < CHUNK_SIZE_X; ++x)
			{
				float t = getTemperature(origin.x + x, origin.z + z);
				float h = getHumidity(origin.x + x, origin.z + z);
				chunk.setColumnTints(x, z, grassTint(t, h), foliageTint(t, h));
			}
}

// ── Génération d'un chunk ───────────────────────────────────────
void TerrainGenerator::generate(Chunk &chunk) const
{
	glm::ivec3 origin = chunk.toWorldPos(0, 0, 0);

	// Hauteurs sur 18x18 (bordure de 1 pour calculer les pentes)
	int heights[CHUNK_SIZE_X + 2][CHUNK_SIZE_Z + 2];
	for(int x = -1; x <= CHUNK_SIZE_X; ++x)
		for(int z = -1; z <= CHUNK_SIZE_Z; ++z)
			heights[x + 1][z + 1] = getHeightAt(origin.x + x, origin.z + z);

	for(int x = 0; x < CHUNK_SIZE_X; ++x)
		{
			for(int z = 0; z < CHUNK_SIZE_Z; ++z)
				{
					int wx = origin.x + x;
					int wz = origin.z + z;
					int h = heights[x + 1][z + 1];
					float temp = getTemperature(wx, wz);
					float hum = getHumidity(wx, wz);
					BiomeType biome = selectBiome(h, temp, hum, wx, wz);
					chunk.setColumnTints(x, z, grassTint(temp, hum),
										 foliageTint(temp, hum));

					int slope = 0;
					slope = std::max(slope, std::abs(heights[x + 2][z + 1] - h));
					slope = std::max(slope, std::abs(heights[x][z + 1] - h));
					slope = std::max(slope, std::abs(heights[x + 1][z + 2] - h));
					slope = std::max(slope, std::abs(heights[x + 1][z] - h));

					// ── Choix des couches de surface ──────────
					BlockType top = BlockType::Grass;
					BlockType under = BlockType::Dirt;
					int depth = 3 + static_cast<int>(hash(wx, wz, 11) & 1u);
					float sn = m_surfaceNoise.noise(wx * 0.08f, wz * 0.08f);

					if(h <= SEA_LEVEL)
						{
							// Fond marin / lacustre
							bool deep = h < SEA_LEVEL - 5;
							top = (deep && sn > 0.1f) ? BlockType::Gravel
													  : BlockType::Sand;
							if(!deep && sn < -0.35f)
								top = BlockType::Dirt;
							under = top == BlockType::Dirt ? BlockType::Dirt
														   : BlockType::Sand;
							depth = 3;
						}
					else if(biome == BiomeType::Beach
							|| biome == BiomeType::Desert)
						{
							top = under = BlockType::Sand;
							depth = 5;
						}
					else if(biome == BiomeType::Amplified && slope < 7)
						{
							// Pics amplifiés : corniches herbeuses jusque sur les aiguilles
							top = BlockType::Grass;
							under = slope >= 3 ? BlockType::Stone : BlockType::Dirt;
						}
					else if(slope >= 4)
						{
							// Falaise : roche nue
							top = under = BlockType::Stone;
							if(sn > 0.55f)
								top = BlockType::Gravel;
						}
					else if(biome == BiomeType::SnowyPeaks)
						{
							top = slope >= 3 ? BlockType::Stone
											 : BlockType::Snow;
							under = BlockType::Stone;
						}
					else if(biome == BiomeType::Mountains)
						{
							if(slope >= 3 || sn > 0.45f)
								top = under = BlockType::Stone;
							else if(h > 138 && sn < -0.2f)
								top = BlockType::Snow;
						}
					else if(biome == BiomeType::SnowyTaiga)
						{
							top = BlockType::Snow;
						}

					// Pentes moyennes : deux couches d'herbe (flancs verts)
					int grassLayers = (top == BlockType::Grass && slope >= 2) ? 2 : 1;

					// ── Remplissage de la colonne ─────────────
					for(int y = 0; y < h; ++y)
						{
							BlockType b;
							if(y == 0
							   || (y <= 2 && (hash(wx * 7 + y, wz, 3) & 1u)))
								b = BlockType::Bedrock;
							else if(y >= h - grassLayers)
								b = top;
							else if(y >= h - depth)
								b = under;
							else
								{
									b = BlockType::Stone;
									float ore = hash01(wx * 131 + y, wz, 5);
									if(y < 70 && ore < 0.006f)
										b = BlockType::Iron_Ore;
									else if(y < 150 && ore > 0.991f)
										b = BlockType::Coal_Ore;
								}
							chunk.setBlock(x, y, z, b);
						}

					// Eau jusqu'au niveau de la mer
					for(int y = h; y < SEA_LEVEL; ++y)
						chunk.setBlock(x, y, z, BlockType::Water);

					// ── Relief amplifié : surplombs et arches ──
					int surfaceY = h - 1;
					float strength = h > SEA_LEVEL ? overhangStrength(wx, wz, slope) : 0.0f;
					if(strength > 0.01f)
						{
							int y0 = std::max(SEA_LEVEL - 6, h - 32);
							int y1 = std::min(CHUNK_SIZE_Y - 12, h + 26);
							for(int y = y1; y >= y0; --y)
								{
									bool solid = overhangDensity(wx, y, wz, h, strength) > 0.0f;
									BlockType cur = chunk.getBlockUnchecked(x, y, z);
									bool curSolid = cur != BlockType::Air && cur != BlockType::Water;
									if(solid && !curSolid)
										chunk.setBlock(x, y, z, BlockType::Stone);
									else if(!solid && curSolid)
										chunk.setBlock(x, y, z, y < SEA_LEVEL ? BlockType::Water : BlockType::Air);
								}
							// Couches de surface recalculées sous chaque ouverture
							int depth = 0;
							bool wet = false;
							surfaceY = -1;
							for(int y = y1; y >= y0; --y)
								{
									BlockType cur = chunk.getBlockUnchecked(x, y, z);
									if(cur == BlockType::Air || cur == BlockType::Water)
										{
											depth = 0;
											wet = cur == BlockType::Water;
											continue;
										}
									if(surfaceY < 0)
										surfaceY = y;
									if(depth <= 3)
										{
											BlockType nb = BlockType::Stone;
											if(wet)
												nb = depth == 0 ? BlockType::Gravel : BlockType::Stone;
											else if(depth < grassLayers)
												nb = top;
											else
												nb = under == BlockType::Sand ? BlockType::Sand
													 : under == BlockType::Dirt ? BlockType::Dirt
																				: BlockType::Stone;
											chunk.setBlock(x, y, z, nb);
										}
									depth++;
								}
							if(surfaceY < 0)
								surfaceY = y0 - 1;
						}

					// ── Désert : cactus et buissons morts ─────
					if(biome == BiomeType::Desert && top == BlockType::Sand && h > SEA_LEVEL + 1
					   && h + 3 < CHUNK_SIZE_Y)
						{
							float r = hash01(wx, wz, 23);
							if(r < 0.0045f && (wx + wz) % 2 == 0)
								{
									int tall = 1 + static_cast<int>(hash(wx, wz, 29) % 3u);
									for(int k = 0; k < tall; ++k)
										chunk.setBlock(x, h + k, z, BlockType::Cactus);
								}
							else if(r < 0.014f)
								chunk.setBlock(x, h, z, BlockType::Dead_Bush);
						}

					// ── Végétation basse ──────────────────────
					h = surfaceY + 1;
					if(surfaceY >= 0 && chunk.getBlockUnchecked(x, surfaceY, z) != BlockType::Grass)
						continue;
					if(top == BlockType::Grass && h < CHUNK_SIZE_Y)
						{
							const BiomeData &bd = getBiomeData(biome);
							float r = hash01(wx, wz, 17);
							if(r < bd.flowerDensity)
								{
									BlockType flowers[] = {
									  BlockType::Dandelion, BlockType::Poppy,
									  BlockType::Cornflower};
									BlockType f
									  = biome == BiomeType::CherryGrove
										  ? BlockType::Pink_Tulip
										  : flowers[hash(wx, wz, 19) % 3];
									chunk.setBlock(x, h, z, f);
								}
							else if(r < bd.flowerDensity + bd.grassDensity)
								{
									chunk.setBlock(x, h, z,
												   BlockType::Short_Grass);
								}
							else if(r > 0.9993f
									&& (biome == BiomeType::Plains || biome == BiomeType::Forest))
								chunk.setBlock(x, h, z, BlockType::Pumpkin);
						}
				}
		}

	placeTrees(chunk);
	placeBoulders(chunk);
	m_structures->place(chunk);
	chunk.updateTopY();
}

// ── Rochers ─────────────────────────────────────────────────────
void TerrainGenerator::placeBoulders(Chunk &chunk) const
{
	constexpr int CELL = 21, REACH = 3;
	glm::ivec3 origin = chunk.toWorldPos(0, 0, 0);
	for(int cx = floorDiv(origin.x - REACH, CELL); cx <= floorDiv(origin.x + CHUNK_SIZE_X + REACH, CELL); ++cx)
		for(int cz = floorDiv(origin.z - REACH, CELL); cz <= floorDiv(origin.z + CHUNK_SIZE_Z + REACH, CELL); ++cz)
			{
				uint32_t rnd = hash(cx, cz, 31);
				int wx = cx * CELL + static_cast<int>(rnd % CELL);
				int wz = cz * CELL + static_cast<int>((rnd >> 8) % CELL);
				ColumnInfo col = getColumn(wx, wz);
				float p = 0.0f;
				switch(col.biome)
					{
					case BiomeType::Plains: p = 0.10f; break;
					case BiomeType::Forest: p = 0.16f; break;
					case BiomeType::Taiga: p = 0.30f; break;
					case BiomeType::SnowyTaiga: p = 0.18f; break;
					case BiomeType::Mountains: p = 0.30f; break;
					case BiomeType::CherryGrove: p = 0.10f; break;
					case BiomeType::Amplified: p = 0.25f; break;
					default: break;
					}
				if(hash01(cx, cz, 37) >= p || col.height <= SEA_LEVEL + 1)
					continue;
				int h = col.height;
				if(std::abs(getHeightAt(wx + 2, wz) - h) > 1 || std::abs(getHeightAt(wx - 2, wz) - h) > 1
				   || std::abs(getHeightAt(wx, wz + 2) - h) > 1 || std::abs(getHeightAt(wx, wz - 2) - h) > 1)
					continue;
				if(m_structures->occupied(wx, wz))
					continue;

				float r = 1.1f + ((rnd >> 16) & 255u) / 255.0f * 1.4f;
				bool mossy = col.biome != BiomeType::Mountains && col.biome != BiomeType::SnowyTaiga;
				for(int dx = -REACH; dx <= REACH; ++dx)
					for(int dz = -REACH; dz <= REACH; ++dz)
						for(int dy = -1; dy <= 3; ++dy)
							{
								int lx = wx + dx - origin.x, lz = wz + dz - origin.z, y = h + dy;
								if(lx < 0 || lx >= CHUNK_SIZE_X || lz < 0 || lz >= CHUNK_SIZE_Z || y >= CHUNK_SIZE_Y)
									continue;
								float fy = (dy + 0.3f) / (r * 0.8f);
								float d = (dx * dx + dz * dz) / (r * r) + fy * fy;
								float jag = (hash01(wx + dx, wz + dz, 41 + dy) - 0.5f) * 0.35f;
								if(d > 1.0f + jag)
									continue;
								BlockType cur = chunk.getBlockUnchecked(lx, y, lz);
								if(dy >= 0 && cur != BlockType::Air && !isPlant(cur))
									continue;
								uint32_t m = hash(wx + dx, (wz + dz) * 31 + dy, 43) % 100u;
								BlockType b = m < 40 ? BlockType::Stone
											  : m < 70 ? BlockType::Cobblestone
														: (mossy ? BlockType::Mossy_Cobblestone : BlockType::Gravel);
								if(dy == 2 && mossy && m < 60)
									b = BlockType::Mossy_Cobblestone;
								chunk.setBlock(lx, y, lz, b);
							}
			}
}

// ── Arbres ──────────────────────────────────────────────────────
void TerrainGenerator::placeTrees(Chunk &chunk) const
{
	glm::ivec3 origin = chunk.toWorldPos(0, 0, 0);
	int minCX = floorDiv(origin.x - TREE_REACH, TREE_CELL);
	int maxCX = floorDiv(origin.x + CHUNK_SIZE_X + TREE_REACH, TREE_CELL);
	int minCZ = floorDiv(origin.z - TREE_REACH, TREE_CELL);
	int maxCZ = floorDiv(origin.z + CHUNK_SIZE_Z + TREE_REACH, TREE_CELL);

	for(int cx = minCX; cx <= maxCX; ++cx)
		for(int cz = minCZ; cz <= maxCZ; ++cz)
			{
				int wx, wz;
				ColumnInfo col;
				if(treeInCell(cx, cz, wx, wz, col))
					placeTree(chunk, col.biome, wx, col.height, wz, hash(cx, cz, 13));
			}
}

bool TerrainGenerator::treeInCell(int cx, int cz, int &wx, int &wz,
								  ColumnInfo &col) const
{
	uint32_t rnd = hash(cx, cz, 7);
	wx = cx * TREE_CELL + static_cast<int>(rnd % TREE_CELL);
	wz = cz * TREE_CELL + static_cast<int>((rnd >> 8) % TREE_CELL);

	col = getColumn(wx, wz);
	const BiomeData &bd = getBiomeData(col.biome);
	if(bd.treeDensity <= 0.0f || hash01(cx, cz, 9) >= bd.treeDensity)
		return false;
	int treeLine = col.biome == BiomeType::Amplified ? getSnowLine(wx, wz) - 8 : TREE_LINE;
	if(col.height <= SEA_LEVEL + 1 || col.height > treeLine)
		return false;

	// Pas d'arbre sur une pente raide
	int h = col.height;
	if(std::abs(getHeightAt(wx + 1, wz) - h) > 1
	   || std::abs(getHeightAt(wx - 1, wz) - h) > 1
	   || std::abs(getHeightAt(wx, wz + 1) - h) > 1
	   || std::abs(getHeightAt(wx, wz - 1) - h) > 1)
		return false;
	// Pas d'arbre dans les villages et autres structures
	return !m_structures->occupied(wx, wz);
}

bool TerrainGenerator::treeNear(int wx, int wz, int radius) const
{
	for(int cx = floorDiv(wx - radius, TREE_CELL); cx <= floorDiv(wx + radius, TREE_CELL); ++cx)
		for(int cz = floorDiv(wz - radius, TREE_CELL); cz <= floorDiv(wz + radius, TREE_CELL); ++cz)
			{
				int tx, tz;
				ColumnInfo col;
				if(treeInCell(cx, cz, tx, tz, col)
				   && (tx - wx) * (tx - wx) + (tz - wz) * (tz - wz) <= radius * radius)
					return true;
			}
	return false;
}

void TerrainGenerator::placeTree(Chunk &chunk, BiomeType biome, int wx,
								 int baseY, int wz, uint32_t rnd) const
{
	glm::ivec3 origin = chunk.toWorldPos(0, 0, 0);

	// Pose un bloc s'il tombe dans ce chunk (sans écraser le terrain)
	auto put = [&](int x, int y, int z, BlockType type, bool force) {
		int lx = x - origin.x;
		int lz = z - origin.z;
		if(lx < 0 || lx >= CHUNK_SIZE_X || lz < 0 || lz >= CHUNK_SIZE_Z
		   || y < 0 || y >= CHUNK_SIZE_Y)
			return;
		BlockType cur = chunk.getBlockUnchecked(lx, y, lz);
		if(cur == BlockType::Air || isPlant(cur)
		   || (force && BlockDatabase::IsTransparent(cur)
			   && cur != BlockType::Water))
			chunk.setBlock(lx, y, lz, type);
	};

	enum class Kind
	{
		Oak,
		Birch,
		Spruce,
		Cherry
	};
	Kind kind = Kind::Oak;
	switch(biome)
		{
		case BiomeType::Forest:
			kind = (rnd % 100) < 30 ? Kind::Birch : Kind::Oak;
			break;
		case BiomeType::Taiga:
		case BiomeType::SnowyTaiga:
		case BiomeType::Mountains: kind = Kind::Spruce; break;
		case BiomeType::CherryGrove:
			kind = (rnd % 100) < 85 ? Kind::Cherry : Kind::Oak;
			break;
		case BiomeType::Amplified:
			kind = (rnd % 100) < 60 ? Kind::Spruce : Kind::Oak;
			break;
		default: kind = Kind::Oak; break;
		}

	rnd = rnd * 1664525u + 1013904223u;

	if(kind == Kind::Oak || kind == Kind::Birch)
		{
			BlockType log = kind == Kind::Oak ? BlockType::Log : BlockType::Birch_Log;
			BlockType leaf
			  = kind == Kind::Oak ? BlockType::Leaves : BlockType::Birch_Leaves;
			int height = (kind == Kind::Oak ? 4 : 5) + static_cast<int>(rnd % 3);
			int topY = baseY + height;

			for(int y = topY - 3; y <= topY; ++y)
				{
					int dy = y - topY;
					int r = dy >= -1 ? 1 : 2;
					for(int dx = -r; dx <= r; ++dx)
						for(int dz = -r; dz <= r; ++dz)
							{
								bool corner = std::abs(dx) == r && std::abs(dz) == r;
								if(corner
								   && (dy == 0
									   || (hash(wx + dx, wz + dz, 100 + y) & 1u)))
									continue;
								put(wx + dx, y, wz + dz, leaf, false);
							}
				}
			for(int y = baseY; y < topY; ++y)
				put(wx, y, wz, log, true);
		}
	else if(kind == Kind::Spruce)
		{
			int height = 7 + static_cast<int>(rnd % 6);
			int topY = baseY + height;
			int leafStart = baseY + 2 + static_cast<int>((rnd >> 4) % 2);
			int maxR = height >= 10 ? 3 : 2;

			// Couches en étages, plus larges vers le bas
			for(int y = topY; y >= leafStart; --y)
				{
					int i = topY - y;
					int r = 0;
					if(i == 0)
						r = 0;
					else
						r = std::min(maxR, 1 + (i - 1) / 2) - (i % 3 == 0 ? 1 : 0);
					r = std::max(r, i == 0 ? 0 : 1);
					for(int dx = -r; dx <= r; ++dx)
						for(int dz = -r; dz <= r; ++dz)
							{
								if(dx * dx + dz * dz > r * r + (r > 1 ? 1 : 0))
									continue;
								put(wx + dx, y, wz + dz, BlockType::Spruce_Leaves,
									false);
							}
				}
			put(wx, topY + 1, wz, BlockType::Spruce_Leaves, false);
			for(int y = baseY; y < topY; ++y)
				put(wx, y, wz, BlockType::Spruce_Log, true);
		}
	else // Cerisier
		{
			int height = 4 + static_cast<int>(rnd % 3);
			int topY = baseY + height;
			// Tronc légèrement incliné
			int ox = static_cast<int>((rnd >> 3) % 3) - 1;
			int oz = static_cast<int>((rnd >> 6) % 3) - 1;
			int cx = wx, cz = wz;
			for(int y = baseY; y < topY; ++y)
				{
					if(y == baseY + height / 2 + 1)
						{
							cx += ox;
							cz += oz;
						}
					put(cx, y, cz, BlockType::Cherry_Log, true);
				}

			// Canopée large et aplatie
			const float R = 3.6f;
			for(int dy = -1; dy <= 2; ++dy)
				for(int dx = -4; dx <= 4; ++dx)
					for(int dz = -4; dz <= 4; ++dz)
						{
							float fy = (dy - 0.4f) / 1.7f;
							float d = (dx * dx + dz * dz) / (R * R) + fy * fy;
							if(d > 1.0f)
								continue;
							if(d > 0.75f && hash01(cx + dx, cz + dz, 200 + dy) < 0.35f)
								continue;
							put(cx + dx, topY + dy, cz + dz,
								BlockType::Cherry_Leaves, false);
						}
			// Fleurs qui pendent sous la canopée
			for(int dx = -3; dx <= 3; ++dx)
				for(int dz = -3; dz <= 3; ++dz)
					{
						float d2 = static_cast<float>(dx * dx + dz * dz);
						if(d2 < 4.0f || d2 > 11.0f)
							continue;
						uint32_t hr = hash(cx + dx, cz + dz, 300);
						if(hr % 100 < 30)
							{
								int len = 1 + static_cast<int>((hr >> 8) % 2);
								for(int k = 1; k <= len; ++k)
									put(cx + dx, topY - 1 - k, cz + dz,
										BlockType::Cherry_Leaves, false);
							}
					}
		}
}
