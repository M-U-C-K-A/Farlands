#include "block.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

// ── Static members ──────────────────────────────────────────────
std::unordered_map<uint8_t, BlockData> BlockDatabase::s_database;
std::vector<std::string> BlockDatabase::s_texturePaths;
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
    
    auto addTexture = [&](const std::string& tex) -> int {
        if (tex.empty()) return 0;
        auto it = std::find(s_texturePaths.begin(), s_texturePaths.end(), tex);
        if (it != s_texturePaths.end()) {
            return static_cast<int>(std::distance(s_texturePaths.begin(), it));
        }
        s_texturePaths.push_back(tex);
        return static_cast<int>(s_texturePaths.size() - 1);
    };

    if (s_texturePaths.empty()) {
        s_texturePaths.push_back(""); // Index 0 represents Air / Transparent
    }

    std::string texGeneral = b.value("texture", "");
    std::string texTop = b.value("textureTop", texGeneral);
    std::string texBottom = b.value("textureBottom", texGeneral);
    std::string texFront = b.value("textureFront", texGeneral);
    std::string texBack = b.value("textureBack", texGeneral);
    std::string texLeft = b.value("textureLeft", texGeneral);
    std::string texRight = b.value("textureRight", texGeneral);

    data.texLayerTop = addTexture(texTop);
    data.texLayerBottom = addTexture(texBottom);
    data.texLayerFront = addTexture(texFront);
    data.texLayerBack = addTexture(texBack);
    data.texLayerLeft = addTexture(texLeft);
    data.texLayerRight = addTexture(texRight);

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
