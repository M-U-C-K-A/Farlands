#include "chunk.h"

#include "../core/types.h"

Chunk::Chunk(int cx, int cz) : m_chunkPos(cx, cz)
{
	m_blocks.fill(BlockType::Air);
	m_grassTint.fill(TINT_NEUTRAL);
	m_foliageTint.fill(TINT_NEUTRAL);
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
	m_lights.clear();
	for(int z = 0; z < CHUNK_SIZE_Z; ++z)
		for(int y = 0; y < m_topY; ++y)
			for(int x = 0; x < CHUNK_SIZE_X; ++x)
				if(BlockDatabase::LightLevel(m_blocks[getIndex(x, y, z)]) > 0)
					m_lights.push_back(static_cast<uint32_t>(x | (z << 4) | (y << 8)));
}
