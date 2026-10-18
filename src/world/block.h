// -----------------------------------------------------------------------------
// Fichier : block.h
// Rôle : Définitions et propriétés de tous les blocs du jeu.
// Gère aussi le chargement de ces données JSON.
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

// ── Block Type Enum ─────────────────────────────────────────────
// IDs match the JSON "id" field
enum class BlockType : uint8_t
{
	Air = 0,
	Dirt = 1,
	Grass = 2,
	Stone = 3,
	Log = 4,
	Plank = 5,
	Glass = 6,
	Leaves = 7,
	Sand = 8,
	Water = 9,
	Cobblestone = 10,
	Bedrock = 11,
	Gravel = 12,
	Iron_Ore = 13,
	Coal_Ore = 14,
	Snow = 15,
	Spruce_Log = 16,
	Spruce_Leaves = 17,
	Birch_Log = 18,
	Birch_Leaves = 19,
	Cherry_Log = 20,
	Cherry_Leaves = 21,
	Short_Grass = 22,
	Dandelion = 23,
	Poppy = 24,
	Cornflower = 25,
	Pink_Tulip = 26,
	// Structures et décor
	Stone_Bricks = 27,
	Mossy_Stone_Bricks = 28,
	Mossy_Cobblestone = 29,
	Sandstone = 30,
	Chiseled_Sandstone = 31,
	Spruce_Planks = 32,
	Cherry_Planks = 33,
	Vermilion_Planks = 34,
	Lantern = 35, // lumineux
	Torch = 36,   // lumineux
	Hay_Bale = 37,
	Farmland = 38,
	Wheat = 39,
	Dirt_Path = 40,
	Bricks = 41,
	Cobweb = 42,
	Dead_Bush = 43,
	Cactus = 44,
	Pumpkin = 45,
	Jack_O_Lantern = 46, // lumineux
	White_Plaster = 47,
	Count
};

/// Géométrie générée pour un bloc
enum class RenderType : uint8_t
{
	Cube,   // cube plein (opaque ou "cutout" si transparent)
	Cross,  // sprite en croix (herbes, fleurs)
	Liquid, // eau : rendue dans la passe translucide
};

/// Teinte de biome appliquée au bloc
enum class TintType : uint8_t
{
	None,
	Grass,
	Foliage,
};

// ── Block Properties ────────────────────────────────────────────
struct BlockData
{
	uint8_t id = 0;
	std::string name = "Air";
	bool isTransparent = true;
	bool isSolid = false;
	float hardness = 0.0f;
	RenderType renderType = RenderType::Cube;
	TintType tint = TintType::None;
	bool waving = false; // ondule au vent (feuillages, plantes)
	uint8_t light = 0;   // lumière émise (0..15, propagée par le maillage)

	// Texture indices into the global texture array
	int texLayerFront = 0;
	int texLayerBack = 0;
	int texLayerTop = 0;
	int texLayerBottom = 0;
	int texLayerLeft = 0;
	int texLayerRight = 0;
};

// ── Block Database (chargée depuis JSON) ────────────────────────
class BlockDatabase
{
  public:
	/// Charge la base de données depuis un fichier JSON.
	/// Doit être appelé une fois au démarrage.
	static void LoadFromFile(const std::string &jsonPath);

	/// Accès aux données d'un bloc par type (O(1), sûr entre threads après
	/// le chargement).
	static const BlockData &Get(BlockType type)
	{
		return s_database[static_cast<uint8_t>(type)];
	}

	/// Raccourcis
	static bool IsTransparent(BlockType type) { return Get(type).isTransparent; }
	static bool IsSolid(BlockType type) { return Get(type).isSolid; }
	static uint8_t LightLevel(BlockType type) { return Get(type).light; }
	/// Cube plein et opaque : cache entièrement les faces voisines
	static bool IsOpaqueCube(BlockType type)
	{
		const BlockData &d = Get(type);
		return type != BlockType::Air && !d.isTransparent
			   && d.renderType == RenderType::Cube;
	}

	/// Vérifie si la DB est chargée
	static bool IsLoaded() { return s_loaded; }

	/// Retourne les chemins de textures uniques pour charger l'Array de Textures
	static const std::vector<std::string> &GetTexturePaths()
	{
		return s_texturePaths;
	}

  private:
	static std::array<BlockData, 256> s_database;
	static std::vector<std::string> s_texturePaths;
	static bool s_loaded;
};
