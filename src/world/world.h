// -----------------------------------------------------------------------------
// Fichier : world.h
// Rôle : Gestion du monde (infini) composé de chunks chargés dynamiquement.
// Utilise le système de biomes pour la génération du terrain.
// -----------------------------------------------------------------------------
#pragma once

#include "../core/types.h"
#include "biome.h"
#include "chunk.h"
#include "chunk_mesh.h"

#include <memory>
#include <string>
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

/// Gère un monde infini de chunks avec biomes et chargement dynamique.
class World {
public:
  /// Configure le répertoire de sauvegarde des chunks
  void setWorldDir(const std::string &dir);

  /// Met à jour les chunks autour du joueur. Retourne true si des chunks ont changé.
  bool updateAroundPlayer(int playerChunkX, int playerChunkZ, int renderRadius);

  /// Récupère un chunk par sa position, ou nullptr
  Chunk *getChunk(int cx, int cz);
  const Chunk *getChunk(int cx, int cz) const;

  /// Retourne le type de bloc aux coordonnées monde absolues
  BlockType getBlockAt(int wx, int wy, int wz) const;

  /// Construit le mesh complet de tous les chunks chargés
  void buildWorldMesh(std::vector<Vertex> &outVertices,
                      std::vector<uint32_t> &outIndices);

  int getRadius() const { return m_radius; }
  int getChunkCount() const { return static_cast<int>(m_chunks.size()); }

  /// Accès au gestionnaire de biomes (pour ImGui display)
  const BiomeManager &getBiomeManager() const { return m_biomeManager; }

private:
  std::unordered_map<glm::ivec2, std::unique_ptr<Chunk>, IVec2Hash> m_chunks;
  std::unordered_map<glm::ivec2, ChunkMesh, IVec2Hash> m_chunkMeshes;
  int m_radius = 0;
  std::string m_worldDir;
  BiomeManager m_biomeManager{42}; // Seed fixe pour reproductibilité

  /// Génère le terrain d'un chunk en utilisant le système de biomes
  void generateTerrain(Chunk &chunk);
};
