#include "block.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

// ── Static members ──────────────────────────────────────────────
std::unordered_map<uint8_t, BlockData> BlockDatabase::s_database;
bool BlockDatabase::s_loaded = false;

// ── Load from JSON ──────────────────────────────────────────────
void BlockDatabase::LoadFromFile(const std::string &jsonPath) {
  std::ifstream file(jsonPath);
  if (!file.is_open()) {
    throw std::runtime_error("BlockDatabase: cannot open " + jsonPath);
  }

  json root = json::parse(file);
  const auto &blocks = root["blocks"];

  for (const auto &b : blocks) {
    BlockData data;
    data.id = b["id"].get<uint8_t>();
    data.name = b["name"].get<std::string>();
    data.isTransparent = b["transparent"].get<bool>();
    data.isSolid = b["solid"].get<bool>();
    data.hardness = b["hardness"].get<float>();
    data.texturePath = b.value("texture", "");

    auto c = b["color"];
    data.color = glm::vec3(c[0].get<float>(), c[1].get<float>(),
                           c[2].get<float>());

    auto ct = b["colorTop"];
    data.colorTop = glm::vec3(ct[0].get<float>(), ct[1].get<float>(),
                              ct[2].get<float>());

    s_database[data.id] = data;
  }

  s_loaded = true;
  std::cout << "[BlockDatabase] Loaded " << s_database.size()
            << " block types from " << jsonPath << std::endl;
}

// ── Get ─────────────────────────────────────────────────────────
const BlockData &BlockDatabase::Get(BlockType type) {
  if (!s_loaded) {
    throw std::runtime_error(
        "BlockDatabase::Get() called before LoadFromFile()!");
  }
  auto it = s_database.find(static_cast<uint8_t>(type));
  if (it == s_database.end()) {
    // Fallback to Air
    return s_database.at(0);
  }
  return it->second;
}
