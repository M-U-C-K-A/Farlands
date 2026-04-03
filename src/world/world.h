// -----------------------------------------------------------------------------
// Fichier : world.h
// Rôle : Gestion du monde entier (composé de multiples Chunks).
// -----------------------------------------------------------------------------
#pragma once

#include "../core/types.h"
#include "chunk.h"
#include "chunk_mesh.h"

#include <memory>
#include <unordered_map>
#include <vector>

/// Hash pour glm::ivec2 (position chunk)
struct IVec2Hash {
  std::size_t operator()(const glm::ivec2 &v) const {
    auto h1 = std::hash<int>{}(v.x);
    auto h2 = std::hash<int>{}(v.y);
    return h1 ^ (h2 << 16);
  }
};

/// Gère un ensemble de chunks et génère le mesh monde complet.
class World {
public:
  /// Génère une grille de chunks centrée autour de (0,0)
  void generate(int radius);

  /// Récupère un chunk par sa position, ou nullptr
  Chunk *getChunk(int cx, int cz);
  const Chunk *getChunk(int cx, int cz) const;

  /// Construit le mesh complet de tous les chunks
  /// avec offset monde et lignes de délimitation
  void buildWorldMesh(std::vector<Vertex> &outVertices,
                      std::vector<uint32_t> &outIndices);

  int getRadius() const { return m_radius; }

private:
  std::unordered_map<glm::ivec2, std::unique_ptr<Chunk>, IVec2Hash> m_chunks;
  int m_radius = 0;

  /// Génère un terrain basique pour un chunk
  void generateTerrain(Chunk &chunk);

  /// Ajoute des lignes de délimitation de chunk au mesh
  void addChunkBorders(std::vector<Vertex> &vertices,
                       std::vector<uint32_t> &indices, int cx, int cz,
                       int terrainHeight);
};
