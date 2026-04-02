#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

// ── Block Type Enum ─────────────────────────────────────────────
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

// ── Block Properties ────────────────────────────────────────────
struct BlockData {
  BlockType id;
  std::string name;
  bool isTransparent;
  bool isSolid;
  float hardness;
  std::string texturePath;
  glm::vec3 color;    // couleur par défaut (côtés)
  glm::vec3 colorTop; // couleur du dessus (si différente)
};

// ── Block Database ──────────────────────────────────────────────
class BlockDatabase {
public:
  static const BlockData &Get(BlockType type) {
    static const std::unordered_map<BlockType, BlockData> database = {
        {BlockType::Air,
         {BlockType::Air, "Air", true, false, 0.0f, "",
          {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}}},
        {BlockType::Dirt,
         {BlockType::Dirt, "Dirt", false, true, 1.5f, "",
          {0.53f, 0.38f, 0.28f}, {0.53f, 0.38f, 0.28f}}},
        {BlockType::Grass,
         {BlockType::Grass, "Grass", false, true, 0.6f, "",
          {0.36f, 0.58f, 0.28f}, {0.46f, 0.72f, 0.31f}}},
        {BlockType::Stone,
         {BlockType::Stone, "Stone", false, true, 1.5f, "",
          {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}}},
        {BlockType::Log,
         {BlockType::Log, "Log", false, true, 2.0f, "",
          {0.45f, 0.30f, 0.15f}, {0.55f, 0.40f, 0.20f}}},
        {BlockType::Plank,
         {BlockType::Plank, "Plank", false, true, 1.5f, "",
          {0.65f, 0.50f, 0.30f}, {0.65f, 0.50f, 0.30f}}},
        {BlockType::Glass,
         {BlockType::Glass, "Glass", true, true, 0.3f, "",
          {0.7f, 0.85f, 0.95f}, {0.7f, 0.85f, 0.95f}}},
        {BlockType::Leaves,
         {BlockType::Leaves, "Leaves", true, true, 0.2f, "",
          {0.25f, 0.55f, 0.20f}, {0.30f, 0.60f, 0.22f}}}};
    return database.at(type);
  }

  static bool IsTransparent(BlockType type) { return Get(type).isTransparent; }
  static bool IsSolid(BlockType type) { return Get(type).isSolid; }
};
