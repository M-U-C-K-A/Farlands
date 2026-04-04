// -----------------------------------------------------------------------------
// Fichier : biome.h
// Rôle : Système de biomes basé sur des cartes de température et d'humidité.
// Chaque biome définit la forme du terrain et les blocs utilisés.
// -----------------------------------------------------------------------------
#pragma once

#include "block.h"
#include "noise.h"

#include <glm/glm.hpp>
#include <string>

// ── Types de biomes ─────────────────────────────────────────────
enum class BiomeType : uint8_t
{
	Ocean = 0,
	Beach,
	Plains,
	Forest,
	Desert,
	Mountains,
	SnowyTundra,
	Swamp,
	Count
};

// ── Propriétés d'un biome ───────────────────────────────────────
struct BiomeData
{
	std::string name;
	glm::vec3 color;         // Couleur d'affichage (ImGui minimap)
	int baseHeight;          // Hauteur de base du terrain
	int heightVariation;     // Amplitude maximale de variation
	BlockType surfaceBlock;  // Bloc de surface (herbe, sable, etc.)
	BlockType subSurface;    // Bloc sous la surface (terre, etc.)
	int subSurfaceDepth;     // Profondeur de la couche sous-surface
	float treeDensity;       // Probabilité d'arbre [0..1] (futur)
};

// ── Gestionnaire de biomes ──────────────────────────────────────
class BiomeManager
{
  public:
	/// Initialise les tables de bruit avec une seed
	explicit BiomeManager(uint32_t seed = 42);

	/// Détermine le biome à une position monde (wx, wz)
	BiomeType getBiomeAt(float wx, float wz) const;

	/// Calcule la hauteur du terrain à (wx, wz) en tenant compte du biome
	int getHeightAt(float wx, float wz) const;

	/// Retourne les données d'un biome
	static const BiomeData &getData(BiomeType type);

	/// Retourne la température à la position [0..1]
	float getTemperature(float wx, float wz) const;
	/// Retourne l'humidité à la position [0..1]
	float getHumidity(float wx, float wz) const;

  private:
	PerlinNoise m_heightNoise;    // Bruit principal pour la hauteur
	PerlinNoise m_detailNoise;    // Bruit de détail (petites variations)
	PerlinNoise m_tempNoise;      // Bruit de température
	PerlinNoise m_humidityNoise;  // Bruit d'humidité
	PerlinNoise m_mountainNoise;  // Bruit de crêtes pour les montagnes

	/// Mélange doux entre deux hauteurs basé sur un facteur de transition
	float blendHeight(float h1, float h2, float t) const;
};
