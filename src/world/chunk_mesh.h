// -----------------------------------------------------------------------------
// Fichier : chunk_mesh.h
// Rôle : Modèles mathématiques et fonctions de création du maillage 3D d'un chunk.
// -----------------------------------------------------------------------------
#pragma once

#include "../core/types.h"
#include "chunk.h"

#include <cstdint>
#include <vector>

class World;


// ── Mesh data container ─────────────────────────────────────────
struct ChunkMesh {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  bool empty() const { return vertices.empty(); }
  void clear() {
    vertices.clear();
    indices.clear();
  }
};

/// Génère le mesh d'un chunk avec face culling.
/// Les couleurs sont tirées de BlockDatabase.
ChunkMesh generateChunkMesh(const Chunk &chunk, const World *world = nullptr);

