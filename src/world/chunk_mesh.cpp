#include "chunk_mesh.h"
#include "block.h"
#include "world.h"

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
    }
    return BlockType::Air;
  };

  // Helper to check if a block is solid (for AO)
  auto isSolid = [&](int x, int y, int z) -> bool {
    return getBlock(x, y, z) != BlockType::Air;
  };

  auto addFace = [&](int x, int y, int z, int faceType, glm::vec3 color,
                     int texX, int texY, float blockTypeID) {
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

    float u0 = static_cast<float>(texX) / 16.0f;
    float v0 = static_cast<float>(texY) / 16.0f;
    float u1 = u0 + (1.0f / 16.0f);
    float v1 = v0 + (1.0f / 16.0f);

    glm::vec2 uv00(u0, v0);
    glm::vec2 uv10(u1, v0);
    glm::vec2 uv01(u0, v1);
    glm::vec2 uv11(u1, v1);

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
        glm::vec3 sideColor = bd.color;
        glm::vec3 topColor = bd.colorTop;
        glm::vec3 botColor = (type == BlockType::Grass) ? BlockDatabase::Get(BlockType::Dirt).color : sideColor;

        int bid = static_cast<int>(type);
        int texX = bid;
        int texSideY = 0;
        int texTopY = 1;

        int botTexX = (type == BlockType::Grass) ? static_cast<int>(BlockType::Dirt) : texX;

        float fType = static_cast<float>(type);
        if (getBlock(x, y, z + 1) == BlockType::Air)
          addFace(x, y, z, 0, sideColor, texX, texSideY, fType);
        if (getBlock(x, y, z - 1) == BlockType::Air)
          addFace(x, y, z, 1, sideColor, texX, texSideY, fType);
        if (getBlock(x, y + 1, z) == BlockType::Air)
          addFace(x, y, z, 2, topColor, texX, texTopY, fType);
        if (getBlock(x, y - 1, z) == BlockType::Air)
          addFace(x, y, z, 3, botColor, botTexX, texSideY, fType);
        if (getBlock(x + 1, y, z) == BlockType::Air)
          addFace(x, y, z, 4, sideColor, texX, texSideY, fType);
        if (getBlock(x - 1, y, z) == BlockType::Air)
          addFace(x, y, z, 5, sideColor, texX, texSideY, fType);
      }
    }
  }

  return mesh;
}
