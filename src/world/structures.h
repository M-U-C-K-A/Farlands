// -----------------------------------------------------------------------------
// Fichier : structures.h
// Rôle : Structures générées : grands villages (voir village.cpp), temples
// du désert, ruines, igloos, sanctuaires des cerisiers et phares. Le monde
// est découpé en régions (une grille large pour les villages, une plus fine
// pour le reste) ; chaque région accueille au plus une structure, choisie de
// façon déterministe selon le biome. Les blocs (plan) sont calculés une fois,
// rangés par chunk, puis posés dans chaque chunk touché.
//
// Méthodes const et sûres entre threads (caches protégés par un mutex).
// -----------------------------------------------------------------------------
#pragma once

#include "chunk.h"

#include <glm/glm.hpp>

#include <climits>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

class TerrainGenerator;

enum class StructureType : uint8_t
{
	None = 0,
	Village,
	DesertTemple,
	Ruins,
	Igloo,
	Shrine,
	Lighthouse,
	Count
};

/// Nom affichable d'un type de structure
const char *structureName(StructureType type);

/// Emplacement d'une structure (calcul léger, sans les blocs)
struct StructureSite
{
	StructureType type = StructureType::None;
	glm::ivec3 center{0}; // y : hauteur du terrain au centre
	int radius = 0;       // emprise (blocs) : pas d'arbres ni de rochers
	int style = 0;        // variante (selon le biome)
	uint32_t seed = 0;
};

/// Blocs d'une structure, en coordonnées monde
struct StructureBlueprint
{
	enum Mode : uint8_t
	{
		Set,      // pose toujours
		Fill,     // seulement dans l'air, l'eau, la végétation (fondations)
		Clear,    // vide (intérieurs)
		ClearSoft // retire seulement la végétation (au-dessus des chemins)
	};
	struct Block
	{
		glm::ivec3 pos;
		BlockType type;
		Mode mode;
	};
	std::vector<Block> blocks;
	glm::ivec3 min{INT_MAX}, max{INT_MIN};
	/// Blocs rangés par chunk (après finalize())
	std::unordered_map<uint64_t, std::vector<Block>> byChunk;

	/// Zone sans arbres ni rochers (routes, bâtiments, champs...), facultative
	int maskX0 = 0, maskZ0 = 0, maskSize = 0;
	std::vector<uint8_t> noTree;

	void add(const glm::ivec3 &p, BlockType t, Mode m)
	{
		if(p.y < 1 || p.y >= CHUNK_SIZE_Y)
			return;
		blocks.push_back({p, t, m});
		min = glm::min(min, p);
		max = glm::max(max, p);
	}
	static uint64_t chunkKey(int cx, int cz)
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) | static_cast<uint32_t>(cz);
	}
	/// Range les blocs par chunk (l'ordre de pose est conservé)
	void finalize()
	{
		for(const Block &b : blocks)
			byChunk[chunkKey(floorDiv(b.pos.x, CHUNK_SIZE_X), floorDiv(b.pos.z, CHUNK_SIZE_Z))].push_back(b);
		blocks.clear();
		blocks.shrink_to_fit();
	}
	/// Pas d'arbre en (x, z) ? (masque ou, à défaut, emprise du plan)
	bool blocksTree(int x, int z) const
	{
		if(maskSize > 0)
			{
				int i = x - maskX0, k = z - maskZ0;
				return i >= 0 && k >= 0 && i < maskSize && k < maskSize && noTree[k * maskSize + i];
			}
		return true;
	}
};

class StructureGenerator
{
  public:
	static constexpr int REGION = 192;          // petites structures : une par région
	static constexpr int MAX_REACH = 64;        // portée maximale autour du centre
	static constexpr int VILLAGE_REGION = 640;  // villages : un au plus par région
	static constexpr int VILLAGE_REACH = 160;   // rayon maximal d'un village

	explicit StructureGenerator(const TerrainGenerator &gen) : m_gen(gen) {}

	/// Structure de la région (rx, rz), si elle existe
	bool siteInRegion(int rx, int rz, StructureSite &out) const;
	/// Village de la grande région (vx, vz), s'il existe
	bool villageInRegion(int vx, int vz, StructureSite &out) const;
	/// Pose dans le chunk les blocs des structures qui le touchent
	void place(Chunk &chunk) const;
	/// (wx, wz) est-il dans l'emprise d'une structure ?
	bool occupied(int wx, int wz) const;
	/// Structure la plus proche (type None : n'importe laquelle)
	bool findNearest(StructureType type, const glm::ivec2 &from, int maxDist,
					 StructureSite &out) const;

  private:
	const TerrainGenerator &m_gen;

	mutable std::mutex m_mutex;
	mutable std::unordered_map<uint64_t, StructureSite> m_sites;
	mutable std::unordered_map<uint64_t, StructureSite> m_villageSites;
	using BlueprintPtr = std::shared_ptr<const StructureBlueprint>;
	struct CachedBlueprint
	{
		BlueprintPtr bp;
		uint64_t lastUse = 0;
	};
	mutable std::unordered_map<uint64_t, CachedBlueprint> m_blueprints;
	// Plans en cours de construction : les autres threads attendent le résultat
	mutable std::unordered_map<uint64_t, std::shared_future<BlueprintPtr>> m_building;
	mutable uint64_t m_useCounter = 0;

	StructureSite computeSite(int rx, int rz) const;
	StructureSite computeVillageSite(int vx, int vz) const;
	/// Village dont l'emprise couvre (wx, wz), s'il y en a un
	bool villageNear(int wx, int wz, int margin, StructureSite &out) const;
	BlueprintPtr blueprint(uint64_t key, const StructureSite &site) const;
	void build(const StructureSite &site, StructureBlueprint &bp) const;
	void stamp(Chunk &chunk, const StructureBlueprint &bp) const;
	void buildDesertTemple(const StructureSite &s, StructureBlueprint &bp) const;
	void buildRuins(const StructureSite &s, StructureBlueprint &bp) const;
	void buildIgloo(const StructureSite &s, StructureBlueprint &bp) const;
	void buildShrine(const StructureSite &s, StructureBlueprint &bp) const;
	void buildLighthouse(const StructureSite &s, StructureBlueprint &bp) const;
};
