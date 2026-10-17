#include "chunk.h"

Chunk::Chunk(int cx, int cz) : m_chunkPos(cx, cz)
{
	m_blocks.fill(BlockType::Air);
	m_grassTint.fill(0xFFFFFFFFu);
	m_foliageTint.fill(0xFFFFFFFFu);
}

void Chunk::setBlock(int x, int y, int z, BlockType type)
{
	if(x >= 0 && x < CHUNK_SIZE_X && y >= 0 && y < CHUNK_SIZE_Y && z >= 0
	   && z < CHUNK_SIZE_Z)
		m_blocks[getIndex(x, y, z)] = type;
}

BlockType Chunk::getBlock(int x, int y, int z) const
{
	if(x >= 0 && x < CHUNK_SIZE_X && y >= 0 && y < CHUNK_SIZE_Y && z >= 0
	   && z < CHUNK_SIZE_Z)
		return m_blocks[getIndex(x, y, z)];
	return BlockType::Air;
}

void Chunk::updateTopY()
{
	m_topY = 0;
	for(int z = 0; z < CHUNK_SIZE_Z; ++z)
		for(int x = 0; x < CHUNK_SIZE_X; ++x)
			for(int y = CHUNK_SIZE_Y - 1; y >= m_topY; --y)
				if(m_blocks[getIndex(x, y, z)] != BlockType::Air)
					{
						m_topY = y + 1;
						break;
					}
}
