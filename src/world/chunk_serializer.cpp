// -----------------------------------------------------------------------------
// Fichier : chunk_serializer.cpp
// Rôle : Implémentation de la sérialisation binaire RLE des chunks.
// -----------------------------------------------------------------------------
#include "chunk_serializer.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

// ── File Path ───────────────────────────────────────────────────
std::string ChunkSerializer::getFilePath(int cx, int cz,
										 const std::string &dir)
{
	return dir + "/chunk_" + std::to_string(cx) + "_" + std::to_string(cz)
		   + ".bin";
}

// ── Save ────────────────────────────────────────────────────────
void ChunkSerializer::save(const Chunk &chunk, const std::string &worldDir)
{
	auto pos = chunk.getChunkPos();
	std::string path = getFilePath(pos.x, pos.y, worldDir);

	std::ofstream file(path, std::ios::binary);
	if(!file.is_open())
		{
			std::cerr << "[ChunkSerializer] Failed to save: " << path
					  << std::endl;
			return;
		}

	// ── Header ──────────────────────────────────────────────
	uint32_t magic = MAGIC;
	uint32_t version = VERSION;
	int32_t cx = pos.x;
	int32_t cz = pos.y;
	file.write(reinterpret_cast<const char *>(&magic), sizeof(magic));
	file.write(reinterpret_cast<const char *>(&version), sizeof(version));
	file.write(reinterpret_cast<const char *>(&cx), sizeof(cx));
	file.write(reinterpret_cast<const char *>(&cz), sizeof(cz));

	// ── RLE Compression ─────────────────────────────────────
	// Format : [blockType (1 byte)] [count (2 bytes)]
	// Max run length = 65535
	const auto &blocks = chunk.getBlocks();

	uint8_t currentType = static_cast<uint8_t>(blocks[0]);
	uint16_t count = 1;

	for(int i = 1; i < CHUNK_VOLUME; ++i)
		{
			uint8_t blockType = static_cast<uint8_t>(blocks[i]);
			if(blockType == currentType && count < 65535)
				{
					count++;
				}
			else
				{
					file.write(reinterpret_cast<const char *>(&currentType),
							   sizeof(currentType));
					file.write(reinterpret_cast<const char *>(&count),
							   sizeof(count));
					currentType = blockType;
					count = 1;
				}
		}
	// Écrire le dernier run
	file.write(reinterpret_cast<const char *>(&currentType),
			   sizeof(currentType));
	file.write(reinterpret_cast<const char *>(&count), sizeof(count));
}

// ── Load ────────────────────────────────────────────────────────
bool ChunkSerializer::load(Chunk &chunk, const std::string &worldDir)
{
	auto pos = chunk.getChunkPos();
	std::string path = getFilePath(pos.x, pos.y, worldDir);

	if(!std::filesystem::exists(path))
		return false;

	std::ifstream file(path, std::ios::binary);
	if(!file.is_open())
		return false;

	// ── Header ──────────────────────────────────────────────
	uint32_t magic, version;
	int32_t cx, cz;
	file.read(reinterpret_cast<char *>(&magic), sizeof(magic));
	file.read(reinterpret_cast<char *>(&version), sizeof(version));
	file.read(reinterpret_cast<char *>(&cx), sizeof(cx));
	file.read(reinterpret_cast<char *>(&cz), sizeof(cz));

	if(magic != MAGIC || version != VERSION)
		{
			std::cerr << "[ChunkSerializer] Invalid file format: " << path
					  << std::endl;
			return false;
		}

	if(cx != pos.x || cz != pos.y)
		{
			std::cerr << "[ChunkSerializer] Chunk position mismatch in: "
					  << path << std::endl;
			return false;
		}

	// ── RLE Decompression ───────────────────────────────────
	int index = 0;
	while(index < CHUNK_VOLUME && file.good())
		{
			uint8_t blockType;
			uint16_t count;
			file.read(reinterpret_cast<char *>(&blockType),
					  sizeof(blockType));
			file.read(reinterpret_cast<char *>(&count), sizeof(count));

			for(uint16_t i = 0; i < count && index < CHUNK_VOLUME; ++i)
				{
					// Convert linear index back to x,y,z
					int x = index % CHUNK_SIZE_X;
					int y = (index / CHUNK_SIZE_X) % CHUNK_SIZE_Y;
					int z = index / (CHUNK_SIZE_X * CHUNK_SIZE_Y);
					chunk.setBlock(x, y, z,
								   static_cast<BlockType>(blockType));
					index++;
				}
		}

	chunk.clearDirty();
	return true;
}
