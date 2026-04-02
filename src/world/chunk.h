#pragma once

#include "block.h"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 256;
constexpr int CHUNK_SIZE_Z = 16;
constexpr int CHUNK_VOLUME = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

class Chunk
{
public:
  Chunk(int cx = 0, int cz = 0);
  ~Chunk() = default;

  // ── Block access ──────────────────────────────────────────────
  void setBlock(int x, int y, int z, BlockType type);
  BlockType getBlock(int x, int y, int z) const;

  // ── Position ──────────────────────────────────────────────────
  glm::ivec2 getChunkPos() const { return m_chunkPos; }

  /// Convertit des coordonnées locales en coordonnées monde
  glm::ivec3 toWorldPos(int lx, int ly, int lz) const
  {
    return {m_chunkPos.x * CHUNK_SIZE_X + lx, ly,
            m_chunkPos.y * CHUNK_SIZE_Z + lz};
  }

  // ── Dirty flag ────────────────────────────────────────────────
  bool isDirty() const { return m_dirty; }
  void clearDirty() { m_dirty = false; }

private:
  std::array<BlockType, CHUNK_VOLUME> m_blocks;
  glm::ivec2 m_chunkPos; // (chunkX, chunkZ) dans la grille monde
  bool m_dirty = true;

  int getIndex(int x, int y, int z) const;
};
