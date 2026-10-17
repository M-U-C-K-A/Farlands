#include "chunk_mesh.h"
#include "block.h"
#include "world.h"
#include "core/logger.h"

// ── Ambient Occlusion ───────────────────────────────────────────
// Calculates vertex AO based on three neighboring blocks (side1, side2, corner)
// Returns a value between 0.0 (fully occluded) and 1.0 (no occlusion)
static float vertexAO(bool side1, bool side2, bool corner)
{
  if(side1 && side2)
    return 0.0f; // Fully occluded (concave corner)
  return 1.0f - (static_cast<float>(side1) + static_cast<float>(side2) + static_cast<float>(corner)) / 3.0f;
}

ChunkMesh generateChunkMesh(const Chunk &chunk, const World *world) {
  ChunkMesh mesh;

  glm::ivec2 cpos = chunk.getChunkPos();
  LOG_TRACE("Generating mesh for chunk (" << cpos.x << ", " << cpos.y << ")...");

  auto getBlock = [&](int x, int y, int z) {
    if (y < 0 || y >= CHUNK_SIZE_Y) return BlockType::Air;
    int cx = cpos.x;
    int cz = cpos.y;
    
    if (x < 0) { cx--; x += CHUNK_SIZE_X; }
    else if (x >= CHUNK_SIZE_X) { cx++; x -= CHUNK_SIZE_X; }

    if (z < 0) { cz--; z += CHUNK_SIZE_Z; }
    else if (z >= CHUNK_SIZE_Z) { cz++; z -= CHUNK_SIZE_Z; }

    if (cx == cpos.x && cz == cpos.y) {
      return chunk.getBlock(x, y, z);
    } else if (world) {
      const Chunk* neighbor = world->getChunk(cx, cz);
      if (neighbor) return neighbor->getBlock(x, y, z);
      // Voisin non chargé : considéré plein pour ne pas générer de "mur"
      // au bord du monde chargé (le chunk est remaillé quand il arrive).
      return BlockType::Stone;
    }
    return BlockType::Air;
  };

  // Helper to check if a block is solid (for AO)
  auto isSolid = [&](int x, int y, int z) -> bool {
    return getBlock(x, y, z) != BlockType::Air;
  };

  auto addFace = [&](int x, int y, int z, int faceType, glm::vec3 color,
                     int texLayer, float blockTypeID) {
    uint32_t idx = static_cast<uint32_t>(mesh.vertices.size());
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);
    float fz = static_cast<float>(z);

    glm::vec3 p000(fx, fy, fz);
    glm::vec3 p100(fx + 1, fy, fz);
    glm::vec3 p110(fx + 1, fy + 1, fz);
    glm::vec3 p010(fx, fy + 1, fz);
    glm::vec3 p001(fx, fy, fz + 1);
    glm::vec3 p101(fx + 1, fy, fz + 1);
    glm::vec3 p111(fx + 1, fy + 1, fz + 1);
    glm::vec3 p011(fx, fy + 1, fz + 1);

    float layer = static_cast<float>(texLayer);
    glm::vec3 uv00(0.0f, 0.0f, layer);
    glm::vec3 uv10(1.0f, 0.0f, layer);
    glm::vec3 uv01(0.0f, 1.0f, layer);
    glm::vec3 uv11(1.0f, 1.0f, layer);



    glm::vec3 normal;
    float ao0, ao1, ao2, ao3;

    switch (faceType) {
    case 0: { // Front (z+1), normal = +Z
      normal = glm::vec3(0, 0, 1);
      // AO pour chaque coin de la face z+1
      ao0 = vertexAO(isSolid(x-1, y,   z+1), isSolid(x,   y-1, z+1), isSolid(x-1, y-1, z+1)); // bottom-left
      ao1 = vertexAO(isSolid(x+1, y,   z+1), isSolid(x,   y-1, z+1), isSolid(x+1, y-1, z+1)); // bottom-right
      ao2 = vertexAO(isSolid(x+1, y,   z+1), isSolid(x,   y+1, z+1), isSolid(x+1, y+1, z+1)); // top-right
      ao3 = vertexAO(isSolid(x-1, y,   z+1), isSolid(x,   y+1, z+1), isSolid(x-1, y+1, z+1)); // top-left
      mesh.vertices.push_back({p001, color, uv01, normal, ao0, blockTypeID});
      mesh.vertices.push_back({p101, color, uv11, normal, ao1, blockTypeID});
      mesh.vertices.push_back({p111, color, uv10, normal, ao2, blockTypeID});
      mesh.vertices.push_back({p011, color, uv00, normal, ao3, blockTypeID});
      break;
    }
    case 1: { // Back (z), normal = -Z
      normal = glm::vec3(0, 0, -1);
      ao0 = vertexAO(isSolid(x+1, y,   z-1), isSolid(x,   y-1, z-1), isSolid(x+1, y-1, z-1));
      ao1 = vertexAO(isSolid(x-1, y,   z-1), isSolid(x,   y-1, z-1), isSolid(x-1, y-1, z-1));
      ao2 = vertexAO(isSolid(x-1, y,   z-1), isSolid(x,   y+1, z-1), isSolid(x-1, y+1, z-1));
      ao3 = vertexAO(isSolid(x+1, y,   z-1), isSolid(x,   y+1, z-1), isSolid(x+1, y+1, z-1));
      mesh.vertices.push_back({p100, color, uv01, normal, ao0, blockTypeID});
      mesh.vertices.push_back({p000, color, uv11, normal, ao1, blockTypeID});
      mesh.vertices.push_back({p010, color, uv10, normal, ao2, blockTypeID});
      mesh.vertices.push_back({p110, color, uv00, normal, ao3, blockTypeID});
      break;
    }
    case 2: { // Top (y+1), normal = +Y
      normal = glm::vec3(0, 1, 0);
      ao0 = vertexAO(isSolid(x-1, y+1, z),   isSolid(x,   y+1, z+1), isSolid(x-1, y+1, z+1));
      ao1 = vertexAO(isSolid(x+1, y+1, z),   isSolid(x,   y+1, z+1), isSolid(x+1, y+1, z+1));
      ao2 = vertexAO(isSolid(x+1, y+1, z),   isSolid(x,   y+1, z-1), isSolid(x+1, y+1, z-1));
      ao3 = vertexAO(isSolid(x-1, y+1, z),   isSolid(x,   y+1, z-1), isSolid(x-1, y+1, z-1));
      mesh.vertices.push_back({p011, color, uv01, normal, ao0, blockTypeID});
      mesh.vertices.push_back({p111, color, uv11, normal, ao1, blockTypeID});
      mesh.vertices.push_back({p110, color, uv10, normal, ao2, blockTypeID});
      mesh.vertices.push_back({p010, color, uv00, normal, ao3, blockTypeID});
      break;
    }
    case 3: { // Bottom (y), normal = -Y
      normal = glm::vec3(0, -1, 0);
      ao0 = vertexAO(isSolid(x-1, y-1, z),   isSolid(x,   y-1, z-1), isSolid(x-1, y-1, z-1));
      ao1 = vertexAO(isSolid(x+1, y-1, z),   isSolid(x,   y-1, z-1), isSolid(x+1, y-1, z-1));
      ao2 = vertexAO(isSolid(x+1, y-1, z),   isSolid(x,   y-1, z+1), isSolid(x+1, y-1, z+1));
      ao3 = vertexAO(isSolid(x-1, y-1, z),   isSolid(x,   y-1, z+1), isSolid(x-1, y-1, z+1));
      mesh.vertices.push_back({p000, color, uv01, normal, ao0, blockTypeID});
      mesh.vertices.push_back({p100, color, uv11, normal, ao1, blockTypeID});
      mesh.vertices.push_back({p101, color, uv10, normal, ao2, blockTypeID});
      mesh.vertices.push_back({p001, color, uv00, normal, ao3, blockTypeID});
      break;
    }
    case 4: { // Right (x+1), normal = +X
      normal = glm::vec3(1, 0, 0);
      ao0 = vertexAO(isSolid(x+1, y,   z+1), isSolid(x+1, y-1, z),   isSolid(x+1, y-1, z+1));
      ao1 = vertexAO(isSolid(x+1, y,   z-1), isSolid(x+1, y-1, z),   isSolid(x+1, y-1, z-1));
      ao2 = vertexAO(isSolid(x+1, y,   z-1), isSolid(x+1, y+1, z),   isSolid(x+1, y+1, z-1));
      ao3 = vertexAO(isSolid(x+1, y,   z+1), isSolid(x+1, y+1, z),   isSolid(x+1, y+1, z+1));
      mesh.vertices.push_back({p101, color, uv01, normal, ao0, blockTypeID});
      mesh.vertices.push_back({p100, color, uv11, normal, ao1, blockTypeID});
      mesh.vertices.push_back({p110, color, uv10, normal, ao2, blockTypeID});
      mesh.vertices.push_back({p111, color, uv00, normal, ao3, blockTypeID});
      break;
    }
    case 5: { // Left (x), normal = -X
      normal = glm::vec3(-1, 0, 0);
      ao0 = vertexAO(isSolid(x-1, y,   z-1), isSolid(x-1, y-1, z),   isSolid(x-1, y-1, z-1));
      ao1 = vertexAO(isSolid(x-1, y,   z+1), isSolid(x-1, y-1, z),   isSolid(x-1, y-1, z+1));
      ao2 = vertexAO(isSolid(x-1, y,   z+1), isSolid(x-1, y+1, z),   isSolid(x-1, y+1, z+1));
      ao3 = vertexAO(isSolid(x-1, y,   z-1), isSolid(x-1, y+1, z),   isSolid(x-1, y+1, z-1));
      mesh.vertices.push_back({p000, color, uv01, normal, ao0, blockTypeID});
      mesh.vertices.push_back({p001, color, uv11, normal, ao1, blockTypeID});
      mesh.vertices.push_back({p011, color, uv10, normal, ao2, blockTypeID});
      mesh.vertices.push_back({p010, color, uv00, normal, ao3, blockTypeID});
      break;
    }
    }

    mesh.indices.push_back(idx + 0);
    mesh.indices.push_back(idx + 1);
    mesh.indices.push_back(idx + 2);
    mesh.indices.push_back(idx + 2);
    mesh.indices.push_back(idx + 3);
    mesh.indices.push_back(idx + 0);
  };

  for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
    for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
      for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        BlockType type = chunk.getBlock(x, y, z);
        if (type == BlockType::Air)
          continue;

        const auto &bd = BlockDatabase::Get(type);
        // Les textures sont déjà colorées : la couleur de vertex est un
        // multiplicateur neutre, sauf la teinte de biome ci-dessous.
        glm::vec3 sideColor(1.0f);
        glm::vec3 topColor(1.0f);
        glm::vec3 botColor(1.0f);

        // Coloration dynamique basée sur le Biome (Température et Humidité)
        if (world != nullptr && (type == BlockType::Grass || type == BlockType::Leaves)) {
            float wx = static_cast<float>(chunk.getChunkPos().x * CHUNK_SIZE_X + x);
            float wz = static_cast<float>(chunk.getChunkPos().y * CHUNK_SIZE_Z + z);
            float temp = world->getBiomeManager().getTemperature(wx, wz);
            float hum = world->getBiomeManager().getHumidity(wx, wz);

            glm::vec3 tint(1.0f);
            
            // Plus chaud -> Herbe plus jaune/asséchée
            tint.r += temp * 0.3f;
            tint.g += temp * 0.1f;
            tint.b -= temp * 0.4f;
            
            // Plus humide -> Herbe plus verte foncée/luxuriante
            tint.r -= hum * 0.3f;
            tint.g += hum * 0.4f;
            tint.b -= hum * 0.1f;
            
            // Normaliser (pas de sur-exposition) puis adoucir : la texture
            // porte déjà la couleur de base, on ne fait que la nuancer.
            tint = glm::max(tint, glm::vec3(0.0f));
            float maxC = glm::max(tint.r, glm::max(tint.g, tint.b));
            if (maxC > 0.0f) tint /= maxC;
            tint = glm::mix(glm::vec3(1.0f), tint, 0.5f);

            topColor *= tint;
            if (type == BlockType::Leaves) {
                sideColor *= tint;
                botColor *= tint;
            }
        }

        int layerFront = bd.texLayerFront;
        int layerBack = bd.texLayerBack;
        int layerTop = bd.texLayerTop;
        int layerBot = bd.texLayerBottom;
        int layerRight = bd.texLayerRight;
        int layerLeft = bd.texLayerLeft;

        // Une face est visible si le voisin est de l'air, ou un bloc
        // transparent d'un autre type (ex. le fond marin sous l'eau).
        auto exposed = [&](BlockType neighbor) {
          return neighbor == BlockType::Air
                 || (neighbor != type && BlockDatabase::IsTransparent(neighbor));
        };

        float fType = static_cast<float>(type);
        if (exposed(getBlock(x, y, z + 1)))
          addFace(x, y, z, 0, sideColor, layerFront, fType);
        if (exposed(getBlock(x, y, z - 1)))
          addFace(x, y, z, 1, sideColor, layerBack, fType);
        if (exposed(getBlock(x, y + 1, z))) {
          addFace(x, y, z, 2, topColor, layerTop, fType);
          // Surface de l'eau : aussi visible depuis le dessous
          if (type == BlockType::Water && y + 1 < CHUNK_SIZE_Y)
            addFace(x, y + 1, z, 3, topColor, layerTop, fType);
        }
        if (exposed(getBlock(x, y - 1, z)))
          addFace(x, y, z, 3, botColor, layerBot, fType);
        if (exposed(getBlock(x + 1, y, z)))
          addFace(x, y, z, 4, sideColor, layerRight, fType);
        if (exposed(getBlock(x - 1, y, z)))
          addFace(x, y, z, 5, sideColor, layerLeft, fType);
      }
    }
  }

  return mesh;
}
