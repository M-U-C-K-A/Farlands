#include "chunk.h"

Chunk::Chunk(int cx, int cz) : m_chunkPos(cx, cz) {
  m_blocks.fill(BlockType::Air);
}

int Chunk::getIndex(int x, int y, int z) const {
  return x + CHUNK_SIZE_X * (y + CHUNK_SIZE_Y * z);
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
  if (x >= 0 && x < CHUNK_SIZE_X && y >= 0 && y < CHUNK_SIZE_Y && z >= 0 &&
      z < CHUNK_SIZE_Z) {
    m_blocks[getIndex(x, y, z)] = type;
    m_dirty = true;
  }
}

BlockType Chunk::getBlock(int x, int y, int z) const {
  if (x >= 0 && x < CHUNK_SIZE_X && y >= 0 && y < CHUNK_SIZE_Y && z >= 0 &&
      z < CHUNK_SIZE_Z) {
    return m_blocks[getIndex(x, y, z)];
  }
  return BlockType::Air;
}
