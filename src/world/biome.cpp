// -----------------------------------------------------------------------------
// Fichier : biome.cpp
// Rôle : Table des propriétés de biomes.
// -----------------------------------------------------------------------------
#include "biome.h"

// ── Base de données des biomes ──────────────────────────────────
static const BiomeData s_biomeData[] = {
  // nom, couleur, surface, arbres, herbe, fleurs
  {"Ocean", {0.15f, 0.35f, 0.75f}, BlockType::Sand, 0.0f, 0.0f, 0.0f},
  {"Beach", {0.95f, 0.90f, 0.60f}, BlockType::Sand, 0.0f, 0.0f, 0.0f},
  {"Plains", {0.45f, 0.75f, 0.30f}, BlockType::Grass, 0.05f, 0.38f, 0.05f},
  {"Forest", {0.20f, 0.55f, 0.15f}, BlockType::Grass, 0.80f, 0.22f, 0.015f},
  {"Desert", {0.90f, 0.80f, 0.45f}, BlockType::Sand, 0.0f, 0.0f, 0.0f},
  {"Mountains", {0.55f, 0.55f, 0.55f}, BlockType::Grass, 0.22f, 0.10f, 0.01f},
  {"Snowy Taiga", {0.80f, 0.88f, 0.92f}, BlockType::Snow, 0.55f, 0.0f, 0.0f},
  {"Taiga", {0.25f, 0.45f, 0.35f}, BlockType::Grass, 0.70f, 0.15f, 0.005f},
  {"Cherry Grove", {0.95f, 0.65f, 0.80f}, BlockType::Grass, 0.55f, 0.28f, 0.06f},
  {"Snowy Peaks", {0.95f, 0.97f, 1.00f}, BlockType::Snow, 0.0f, 0.0f, 0.0f},
  {"Amplified Peaks", {0.40f, 0.62f, 0.38f}, BlockType::Grass, 0.30f, 0.30f, 0.03f},
};

static_assert(sizeof(s_biomeData) / sizeof(s_biomeData[0])
				== static_cast<size_t>(BiomeType::Count),
			  "Biome table out of sync with BiomeType");

const BiomeData &getBiomeData(BiomeType type)
{
	return s_biomeData[static_cast<int>(type)];
}
