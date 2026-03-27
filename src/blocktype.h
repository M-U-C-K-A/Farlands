#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

// 1. Enumération propre et typée
enum class BlockType : uint8_t {
  Air,
  Dirt,
  Grass,
  Stone,
  Log,
  Plank,
  Glass,
  Leaves,
  Count
};

// 2. Structure regroupant les propriétés d'un bloc
struct BlockData {
  BlockType id;
  std::string name;
  bool isTransparent;
  bool isSolid;
  float hardness;
  std::string path;
};

// 3. Gestionnaire de base de données des blocs
class BlockDatabase {
public:
  static const BlockData &GetFileData(BlockType type) {
    static const std::unordered_map<BlockType, BlockData> database = {
        {BlockType::Air, {BlockType::Air, "Air", true, false, 0.0f, ""}},
        {BlockType::Dirt, {BlockType::Dirt, "Dirt", false, true, 1.5f, ""}},
        {BlockType::Grass, {BlockType::Grass, "Grass", false, true, 0.6f, ""}},
        {BlockType::Stone, {BlockType::Stone, "Stone", false, true, 1.5f, ""}},
        {BlockType::Log, {BlockType::Log, "Log", false, true, 2.0f, ""}},
        {BlockType::Plank, {BlockType::Plank, "Plank", false, true, 1.5f, ""}},
        {BlockType::Glass, {BlockType::Glass, "Glass", true, true}},
        {BlockType::Leaves, {BlockType::Leaves, "Leaves", true, true}}};
    return database.at(type);
  }

  static bool IsTransparent(BlockType type) {
    return GetFileData(type).isTransparent;
  }
};
