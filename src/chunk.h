#pragma once
#include "blocktype.h"
#include <array>
#include <cstdint>
#include <vector>

#include "cube.h" // For Vertex

constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 256;
constexpr int CHUNK_SIZE_Z = 16;
constexpr int CHUNK_VOLUME = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

class Chunk {
public:
  Chunk();
  ~Chunk() = default;

  void setBlock(int x, int y, int z, BlockType type);
  BlockType getBlock(int x, int y, int z) const;

  void generateMesh();

  const std::vector<Vertex> &getVertices() const { return m_vertices; }
  const std::vector<uint32_t> &getIndices() const { return m_indices; }

private:
  std::array<BlockType, CHUNK_VOLUME> m_blocks;
  std::vector<Vertex> m_vertices;
  std::vector<uint32_t> m_indices;

  int getIndex(int x, int y, int z) const;
};
