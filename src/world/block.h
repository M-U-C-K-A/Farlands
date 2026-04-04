// -----------------------------------------------------------------------------
// Fichier : block.h
// Rôle : Définitions et propriétés de tous les blocs du jeu.
// Gère aussi le chargement de ces données JSON.
// -----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

// ── Block Type Enum ─────────────────────────────────────────────
// IDs match the JSON "id" field
enum class BlockType : uint8_t {
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
  Count
};

// ── Block Properties ────────────────────────────────────────────
struct BlockData {
  uint8_t id;
  std::string name;
  bool isTransparent;
  bool isSolid;
  float hardness;
  // Texture indices into the global texture array
  int texLayerFront;
  int texLayerBack;
  int texLayerTop;
  int texLayerBottom;
  int texLayerLeft;
  int texLayerRight;

  glm::vec3 color;    // couleur par défaut (côtés)
  glm::vec3 colorTop; // couleur du dessus (si différente)
};

// ── Block Database (chargée depuis JSON) ────────────────────────
class BlockDatabase {
public:
  /// Charge la base de données depuis un fichier JSON.
  /// Doit être appelé une fois au démarrage.
  static void LoadFromFile(const std::string &jsonPath);

  /// Accès aux données d'un bloc par type.
  static const BlockData &Get(BlockType type);

  /// Raccourcis
  static bool IsTransparent(BlockType type) { return Get(type).isTransparent; }
  static bool IsSolid(BlockType type) { return Get(type).isSolid; }

  /// Vérifie si la DB est chargée
  static bool IsLoaded() { return s_loaded; }

  /// Retourne les chemins de textures uniques pour charger l'Array de Textures
  static const std::vector<std::string>& GetTexturePaths() { return s_texturePaths; }

private:
  static std::unordered_map<uint8_t, BlockData> s_database;
  static std::vector<std::string> s_texturePaths;
  static bool s_loaded;
};
