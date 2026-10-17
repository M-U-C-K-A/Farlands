// -----------------------------------------------------------------------------
// Fichier : chunk.h
// Rôle : Définition d'un tronçon (Chunk) de la carte, stockant les blocs en mémoire.
// Un chunk est immuable une fois généré et publié (lecture concurrente sûre
// depuis les threads de maillage).
// -----------------------------------------------------------------------------
#pragma once

#include "block.h"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 256;
constexpr int CHUNK_SIZE_Z = 16;
constexpr int CHUNK_VOLUME = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
constexpr int CHUNK_AREA = CHUNK_SIZE_X * CHUNK_SIZE_Z;

/// Division entière arrondie vers -inf (coordonnée monde -> chunk)
inline int floorDiv(int a, int b)
{
	return (a >= 0) ? (a / b) : ((a - b + 1) / b);
}

class Chunk
{
  public:
	// Constructeur : initialise un chunk avec ses coordonnées grille (cx, cz).
	Chunk(int cx = 0, int cz = 0);
	~Chunk() = default;

	// Modifie le type de bloc à une position locale (x, y, z) au sein du chunk
	void setBlock(int x, int y, int z, BlockType type);
	// Retourne le type de bloc à la position locale demandée
	BlockType getBlock(int x, int y, int z) const;
	/// Accès sans vérification de bornes (coordonnées supposées valides)
	BlockType getBlockUnchecked(int x, int y, int z) const
	{
		return m_blocks[getIndex(x, y, z)];
	}

	// Renvoie la position du chunk dans la grille globale du monde
	glm::ivec2 getChunkPos() const { return m_chunkPos; }

	/// Convertit des coordonnées locales en coordonnées monde
	glm::ivec3 toWorldPos(int lx, int ly, int lz) const
	{
		return {m_chunkPos.x * CHUNK_SIZE_X + lx, ly,
				m_chunkPos.y * CHUNK_SIZE_Z + lz};
	}

	// ── Teintes de biome par colonne (RGBA8) ───────────────────
	void setColumnTints(int x, int z, uint32_t grass, uint32_t foliage)
	{
		m_grassTint[x + z * CHUNK_SIZE_X] = grass;
		m_foliageTint[x + z * CHUNK_SIZE_X] = foliage;
	}
	uint32_t getGrassTint(int x, int z) const
	{
		return m_grassTint[x + z * CHUNK_SIZE_X];
	}
	uint32_t getFoliageTint(int x, int z) const
	{
		return m_foliageTint[x + z * CHUNK_SIZE_X];
	}

	/// Hauteur du plus haut bloc non vide (+1), pour borner le maillage
	int getTopY() const { return m_topY; }
	void updateTopY();

	/// Modifié par le joueur : à sauvegarder sur disque
	bool isModified() const { return m_modified; }
	void setModified(bool m) { m_modified = m; }

	/// Accès direct au tableau de blocs (pour la sérialisation)
	const std::array<BlockType, CHUNK_VOLUME> &getBlocks() const
	{
		return m_blocks;
	}

  private:
	std::array<BlockType, CHUNK_VOLUME> m_blocks;
	std::array<uint32_t, CHUNK_AREA> m_grassTint;
	std::array<uint32_t, CHUNK_AREA> m_foliageTint;
	glm::ivec2 m_chunkPos; // (chunkX, chunkZ) dans la grille monde
	int m_topY = CHUNK_SIZE_Y;
	bool m_modified = false;

	static int getIndex(int x, int y, int z)
	{
		return x + CHUNK_SIZE_X * (y + CHUNK_SIZE_Y * z);
	}
};
