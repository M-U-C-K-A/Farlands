#include "block.h"

#include "core/logger.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

// ── Static members ──────────────────────────────────────────────
std::array<BlockData, 256> BlockDatabase::s_database;
std::vector<std::string> BlockDatabase::s_texturePaths;
bool BlockDatabase::s_loaded = false;

static RenderType parseRenderType(const std::string &s)
{
	if(s == "cross")
		return RenderType::Cross;
	if(s == "liquid")
		return RenderType::Liquid;
	return RenderType::Cube;
}

static TintType parseTint(const std::string &s)
{
	if(s == "grass")
		return TintType::Grass;
	if(s == "foliage")
		return TintType::Foliage;
	return TintType::None;
}

// ── Load from JSON ──────────────────────────────────────────────
void BlockDatabase::LoadFromFile(const std::string &jsonPath)
{
	std::ifstream file(jsonPath);
	if(!file.is_open())
		throw std::runtime_error("BlockDatabase: cannot open " + jsonPath);

	json root = json::parse(file);
	const auto &blocks = root["blocks"];

	s_texturePaths.clear();
	s_texturePaths.push_back(""); // Index 0 represents Air / Transparent

	auto addTexture = [&](const std::string &tex) -> int {
		if(tex.empty())
			return 0;
		auto it = std::find(s_texturePaths.begin(), s_texturePaths.end(), tex);
		if(it != s_texturePaths.end())
			return static_cast<int>(std::distance(s_texturePaths.begin(), it));
		s_texturePaths.push_back(tex);
		return static_cast<int>(s_texturePaths.size() - 1);
	};

	int count = 0;
	for(const auto &b : blocks)
		{
			BlockData data;
			data.id = b["id"].get<uint8_t>();
			data.name = b["name"].get<std::string>();
			data.isTransparent = b.value("transparent", false);
			data.isSolid = b.value("solid", true);
			data.hardness = b.value("hardness", 1.0f);
			data.renderType = parseRenderType(b.value("renderType", "cube"));
			data.tint = parseTint(b.value("tint", "none"));
			data.waving = b.value("waving", false);
			data.light = static_cast<uint8_t>(std::clamp(b.value("light", 0), 0, 15));

			std::string texGeneral = b.value("texture", "");
			data.texLayerTop = addTexture(b.value("textureTop", texGeneral));
			data.texLayerBottom
			  = addTexture(b.value("textureBottom", texGeneral));
			data.texLayerFront = addTexture(b.value("textureFront", texGeneral));
			data.texLayerBack = addTexture(b.value("textureBack", texGeneral));
			data.texLayerLeft = addTexture(b.value("textureLeft", texGeneral));
			data.texLayerRight = addTexture(b.value("textureRight", texGeneral));

			s_database[data.id] = data;
			count++;
		}

	s_loaded = true;
	LOG_INFO("BlockDatabase: " << count << " block types, "
							   << s_texturePaths.size() - 1
							   << " textures from " << jsonPath);
}
