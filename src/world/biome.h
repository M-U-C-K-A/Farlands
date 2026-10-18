// -----------------------------------------------------------------------------
// Fichier : biome.h
// Rôle : Types de biomes et leurs propriétés (affichage, végétation).
// La sélection du biome et la forme du terrain sont dans TerrainGenerator.
// -----------------------------------------------------------------------------
#pragma once

#include "block.h"

#include <glm/glm.hpp>
#include <string>

/// Niveau de la mer (l'eau remplit les colonnes plus basses)
constexpr int SEA_LEVEL = 62;

// ── Types de biomes ─────────────────────────────────────────────
enum class BiomeType : uint8_t
{
	Ocean = 0,
	Beach,
	Plains,
	Forest,
	Desert,
	Mountains,
	SnowyTaiga,
	Taiga,
	CherryGrove,
	SnowyPeaks,
	Amplified, // pics amplifiés : aiguilles vertigineuses, surplombs
	Count
};

// ── Propriétés d'un biome ───────────────────────────────────────
struct BiomeData
{
	std::string name;
	glm::vec3 color;        // Couleur d'affichage (ImGui)
	BlockType surfaceBlock; // Bloc de surface principal
	float treeDensity;      // Probabilité d'un arbre par cellule (5x5 blocs)
	float grassDensity;     // Probabilité d'herbe haute sur l'herbe
	float flowerDensity;    // Probabilité de fleur sur l'herbe
};

/// Retourne les données d'un biome
const BiomeData &getBiomeData(BiomeType type);
