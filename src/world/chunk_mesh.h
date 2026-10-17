// -----------------------------------------------------------------------------
// Fichier : chunk_mesh.h
// Rôle : Construction du maillage 3D d'un chunk (faces visibles, occlusion
// ambiante, sprites de plantes, eau). Exécuté sur les threads de travail :
// ne lit que des chunks immuables partagés.
// -----------------------------------------------------------------------------
#pragma once

#include "../core/types.h"
#include "chunk.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

// ── Mesh data container ─────────────────────────────────────────
struct ChunkMeshData
{
	glm::ivec2 chunkPos{0};
	std::vector<Vertex> vertices;
	// Indices : opaques, puis "cutout" (feuillages, plantes, vitres), puis eau
	std::vector<uint32_t> indices;
	uint32_t opaqueIndexCount = 0;
	uint32_t cutoutIndexCount = 0;
	uint32_t waterIndexCount = 0;
	float minY = 0.0f;
	float maxY = 0.0f;

	bool empty() const { return indices.empty(); }
};

/// Le chunk à mailler et ses 8 voisins (index (dx + 1) + (dz + 1) * 3)
struct ChunkNeighborhood
{
	std::array<std::shared_ptr<const Chunk>, 9> chunks;

	const Chunk &center() const { return *chunks[4]; }

	/// Bloc en coordonnées locales au chunk central (x, z dans [-16, 31])
	BlockType get(int x, int y, int z) const
	{
		if(y < 0)
			return BlockType::Bedrock;
		if(y >= CHUNK_SIZE_Y)
			return BlockType::Air;
		int ix = x < 0 ? 0 : (x >= CHUNK_SIZE_X ? 2 : 1);
		int iz = z < 0 ? 0 : (z >= CHUNK_SIZE_Z ? 2 : 1);
		const Chunk *c = chunks[ix + iz * 3].get();
		if(!c)
			return BlockType::Stone; // voisin absent : considéré plein
		return c->getBlockUnchecked(x - (ix - 1) * CHUNK_SIZE_X, y,
									z - (iz - 1) * CHUNK_SIZE_Z);
	}
};

/// Génère le mesh du chunk central du voisinage.
ChunkMeshData buildChunkMesh(const ChunkNeighborhood &n);
