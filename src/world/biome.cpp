// -----------------------------------------------------------------------------
// Fichier : biome.cpp
// Rôle : Implémentation du système de biomes.
// Utilise des cartes de bruit (température, humidité) pour sélectionner
// les biomes et calculer la hauteur du terrain.
// -----------------------------------------------------------------------------
#include "biome.h"

#include <algorithm>
#include <cmath>

// ── Base de données des biomes ──────────────────────────────────
static const BiomeData s_biomeData[] = {
  // Ocean
  {"Ocean",
   {0.15f, 0.35f, 0.75f},
   28,  2,
   BlockType::Sand, BlockType::Sand, 3,
   0.0f},

  // Beach
  {"Beach",
   {0.95f, 0.90f, 0.60f},
   32,  2,
   BlockType::Sand, BlockType::Sand, 4,
   0.0f},

  // Plains
  {"Plains",
   {0.45f, 0.75f, 0.30f},
   38, 8,
   BlockType::Grass, BlockType::Dirt, 4,
   0.02f},

  // Forest
  {"Forest",
   {0.20f, 0.55f, 0.15f},
   40, 12,
   BlockType::Grass, BlockType::Dirt, 4,
   0.15f},

  // Desert
  {"Desert",
   {0.90f, 0.80f, 0.45f},
   36, 6,
   BlockType::Sand, BlockType::Sand, 6,
   0.0f},

  // Mountains
  {"Mountains",
   {0.55f, 0.55f, 0.55f},
   50, 40,
   BlockType::Stone, BlockType::Stone, 8,
   0.0f},

  // SnowyTundra
  {"Snowy Tundra",
   {0.85f, 0.90f, 0.95f},
   36, 5,
   BlockType::Grass, BlockType::Dirt, 3, // Idéalement Snow, mais on n'a pas le bloc
   0.0f},

  // Swamp
  {"Swamp",
   {0.35f, 0.45f, 0.25f},
   31, 3,
   BlockType::Grass, BlockType::Dirt, 5,
   0.05f},
};

// ── Constructeur ────────────────────────────────────────────────
BiomeManager::BiomeManager(uint32_t seed)
	: m_heightNoise(seed), m_detailNoise(seed + 1),
	  m_tempNoise(seed + 100), m_humidityNoise(seed + 200),
	  m_mountainNoise(seed + 300)
{}

// ── Température [0, 1] ──────────────────────────────────────────
float BiomeManager::getTemperature(float wx, float wz) const
{
	// Grande échelle (biomes larges) + un peu de variation locale
	float temp = m_tempNoise.octaveNoise(wx * 0.002f, wz * 0.002f, 4, 2.0f, 0.5f);
	temp += m_detailNoise.noise(wx * 0.01f, wz * 0.01f) * 0.1f;
	return std::clamp((temp + 1.0f) * 0.5f, 0.0f, 1.0f); // Remap [-1,1] → [0,1]
}

// ── Humidité [0, 1] ─────────────────────────────────────────────
float BiomeManager::getHumidity(float wx, float wz) const
{
	float hum = m_humidityNoise.octaveNoise(wx * 0.0025f, wz * 0.0025f, 4, 2.0f, 0.5f);
	return std::clamp((hum + 1.0f) * 0.5f, 0.0f, 1.0f);
}

// ── Sélection du biome ──────────────────────────────────────────
BiomeType BiomeManager::getBiomeAt(float wx, float wz) const
{
	float temp = getTemperature(wx, wz);
	float humidity = getHumidity(wx, wz);

	// Calcul de la hauteur brute pour détecter océan/plage/montagne
	float continentality = m_heightNoise.octaveNoise(
	  wx * 0.003f, wz * 0.003f, 5, 2.0f, 0.45f);

	// Océan : zones très basses
	if(continentality < -0.3f)
		return BiomeType::Ocean;

	// Plage : transition terre/océan
	if(continentality < -0.15f)
		return BiomeType::Beach;

	// Montagnes : zones très hautes en continentality + crêtes
	float mountain = m_mountainNoise.octaveNoise(
	  wx * 0.005f, wz * 0.005f, 4, 2.0f, 0.5f);
	if(continentality > 0.35f && mountain > 0.1f)
		return BiomeType::Mountains;

	// Biomes basés sur température/humidité
	if(temp < 0.25f)
		return BiomeType::SnowyTundra;

	if(temp > 0.7f && humidity < 0.35f)
		return BiomeType::Desert;

	if(humidity > 0.6f && temp < 0.5f)
		return BiomeType::Swamp;

	if(humidity > 0.45f)
		return BiomeType::Forest;

	return BiomeType::Plains;
}

// ── Hauteur du terrain ──────────────────────────────────────────
int BiomeManager::getHeightAt(float wx, float wz) const
{
	// 1. Base undulating landscape
	float lowFreq = m_heightNoise.octaveNoise(wx * 0.002f, wz * 0.002f, 4, 2.0f, 0.5f);
	
	// 2. High-freq hills
	float highFreq = m_detailNoise.octaveNoise(wx * 0.015f, wz * 0.015f, 4, 2.0f, 0.5f);
	
	// 3. Mountains/Ridges
	float ridgeVal = m_mountainNoise.octaveNoise(wx * 0.005f, wz * 0.005f, 5, 2.0f, 0.5f);
	ridgeVal = 1.0f - std::abs(ridgeVal); // absolute value creates mountains/crevasses
	ridgeVal = ridgeVal * ridgeVal; // emphasize the ridge peaks

	// Combine smoothly
	float baseHeight = 45.0f;
	baseHeight += lowFreq * 20.0f; 

	// Map lowFreq [-1, 1] into a mountain weight [0, 1] softly
	float mountainWeight = std::clamp((lowFreq + 0.1f) * 2.0f, 0.0f, 1.0f);
	
	// Add hills and mountains
	float h = baseHeight;
	h += highFreq * 6.0f; // small hills everywhere
	h += ridgeVal * 45.0f * mountainWeight; // majestic mountains

	// Oceans: lower the terrain when lowFreq is very negative
	float oceanWeight = std::clamp((-0.1f - lowFreq) * 2.5f, 0.0f, 1.0f);
	if (oceanWeight > 0.0f) {
		h -= 25.0f * oceanWeight; 
	}

	return static_cast<int>(std::clamp(h, 5.0f, 250.0f));
}

// ── Données biome ───────────────────────────────────────────────
const BiomeData &BiomeManager::getData(BiomeType type)
{
	return s_biomeData[static_cast<int>(type)];
}

float BiomeManager::blendHeight(float h1, float h2, float t) const
{
	t = std::clamp(t, 0.0f, 1.0f);
	// Smoothstep pour une transition plus douce
	t = t * t * (3.0f - 2.0f * t);
	return h1 * (1.0f - t) + h2 * t;
}
