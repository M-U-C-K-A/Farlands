// -----------------------------------------------------------------------------
// Fichier : terrain_generator.h
// Rôle : Génération procédurale du terrain : relief (continents, lacs,
// montagnes à crêtes), biomes, surface (neige, falaises, plages), minerais,
// arbres et végétation.
//
// Toutes les méthodes sont const et sans état mutable : elles peuvent être
// appelées en parallèle depuis plusieurs threads.
// -----------------------------------------------------------------------------
#pragma once

#include "biome.h"
#include "chunk.h"
#include "noise.h"

#include <cstdint>
#include <glm/glm.hpp>

/// Informations de surface d'une colonne (x, z)
struct ColumnInfo
{
	int height;        // nombre de blocs pleins (surface à y = height - 1)
	BiomeType biome;
	float temperature; // [0, 1]
	float humidity;    // [0, 1]
};

class TerrainGenerator
{
  public:
	explicit TerrainGenerator(uint32_t seed = 42);

	/// Remplit un chunk vide (terrain, eau, arbres, plantes, teintes)
	void generate(Chunk &chunk) const;
	/// Calcule uniquement les teintes de biome (chunk chargé depuis le disque)
	void computeTints(Chunk &chunk) const;

	/// Hauteur du terrain (nombre de blocs pleins) en (wx, wz)
	int getHeightAt(int wx, int wz) const;
	/// Biome, température, humidité d'une colonne
	ColumnInfo getColumn(int wx, int wz) const;
	BiomeType getBiomeAt(int wx, int wz) const { return getColumn(wx, wz).biome; }
	float getTemperature(int wx, int wz) const;
	float getHumidity(int wx, int wz) const;
	/// Altitude de la limite des neiges éternelles en (wx, wz)
	int getSnowLine(int wx, int wz) const;

	/// Un tronc d'arbre se trouve-t-il à moins de `radius` blocs de (wx, wz) ?
	bool treeNear(int wx, int wz, int radius) const;

	uint32_t getSeed() const { return m_seed; }

	/// Hash entier déterministe (x, z, sel) -> [0, 2^32)
	uint32_t hash(int x, int z, uint32_t salt) const;
	/// Hash -> [0, 1)
	float hash01(int x, int z, uint32_t salt) const;

  private:
	uint32_t m_seed;
	PerlinNoise m_continentNoise;
	PerlinNoise m_erosionNoise;
	PerlinNoise m_ridgeNoise;
	PerlinNoise m_detailNoise;
	PerlinNoise m_hillNoise;
	PerlinNoise m_lakeNoise;
	PerlinNoise m_tempNoise;
	PerlinNoise m_humidityNoise;
	PerlinNoise m_cherryNoise;
	PerlinNoise m_surfaceNoise;

	BiomeType selectBiome(int height, float temp, float humidity, int wx,
						  int wz) const;
	uint32_t grassTint(float temp, float humidity) const;
	uint32_t foliageTint(float temp, float humidity) const;

	/// Arbre de la cellule (cx, cz) : position du tronc si présent
	bool treeInCell(int cx, int cz, int &wx, int &wz, ColumnInfo &col) const;
	void placeTrees(Chunk &chunk) const;
	void placeTree(Chunk &chunk, BiomeType biome, int wx, int baseY, int wz,
				   uint32_t rnd) const;
};
