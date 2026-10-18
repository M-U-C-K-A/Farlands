// -----------------------------------------------------------------------------
// Fichier : structures.h
// Rôle : Structures générées : villages (maisons, puits, champs, lampadaires,
// chemins), temples du désert, ruines, igloos, sanctuaires des cerisiers et
// phares. Le monde est découpé en régions ; chaque région accueille au plus
// une structure, choisie de façon déterministe selon le biome. Les blocs
// (plan) sont calculés une fois puis posés dans chaque chunk touché.
//
// Méthodes const et sûres entre threads (caches protégés par un mutex).
// -----------------------------------------------------------------------------
#pragma once

#include "chunk.h"

#include <glm/glm.hpp>

#include <climits>
#include <cstdint>
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

	void add(const glm::ivec3 &p, BlockType t, Mode m)
	{
		if(p.y < 1 || p.y >= CHUNK_SIZE_Y)
			return;
		blocks.push_back({p, t, m});
		min = glm::min(min, p);
		max = glm::max(max, p);
	}
};

class StructureGenerator
{
  public:
	static constexpr int REGION = 192;   // une structure au plus par région
	static constexpr int MAX_REACH = 64; // portée maximale autour du centre

	explicit StructureGenerator(const TerrainGenerator &gen) : m_gen(gen) {}

	/// Structure de la région (rx, rz), si elle existe
	bool siteInRegion(int rx, int rz, StructureSite &out) const;
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
	struct CachedBlueprint
	{
		std::shared_ptr<const StructureBlueprint> bp;
		uint64_t lastUse = 0;
	};
	mutable std::unordered_map<uint64_t, CachedBlueprint> m_blueprints;
	mutable uint64_t m_useCounter = 0;

	StructureSite computeSite(int rx, int rz) const;
	std::shared_ptr<const StructureBlueprint> blueprint(int rx, int rz,
														const StructureSite &site) const;
	void build(const StructureSite &site, StructureBlueprint &bp) const;

	void buildVillage(const StructureSite &s, StructureBlueprint &bp) const;
	void buildDesertTemple(const StructureSite &s, StructureBlueprint &bp) const;
	void buildRuins(const StructureSite &s, StructureBlueprint &bp) const;
	void buildIgloo(const StructureSite &s, StructureBlueprint &bp) const;
	void buildShrine(const StructureSite &s, StructureBlueprint &bp) const;
	void buildLighthouse(const StructureSite &s, StructureBlueprint &bp) const;
};
