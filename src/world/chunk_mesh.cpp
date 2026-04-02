#include "chunk_mesh.h"
#include "block.h"

ChunkMesh generateChunkMesh(const Chunk &chunk) {
  ChunkMesh mesh;

  auto addFace = [&](int x, int y, int z, int faceType, glm::vec3 color) {
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

    switch (faceType) {
    case 0: // Front (z+1)
      mesh.vertices.push_back({p001, color});
      mesh.vertices.push_back({p101, color});
      mesh.vertices.push_back({p111, color});
      mesh.vertices.push_back({p011, color});
      break;
    case 1: // Back (z)
      mesh.vertices.push_back({p100, color});
      mesh.vertices.push_back({p000, color});
      mesh.vertices.push_back({p010, color});
      mesh.vertices.push_back({p110, color});
      break;
    case 2: // Top (y+1)
      mesh.vertices.push_back({p011, color});
      mesh.vertices.push_back({p111, color});
      mesh.vertices.push_back({p110, color});
      mesh.vertices.push_back({p010, color});
      break;
    case 3: // Bottom (y)
      mesh.vertices.push_back({p000, color});
      mesh.vertices.push_back({p100, color});
      mesh.vertices.push_back({p101, color});
      mesh.vertices.push_back({p001, color});
      break;
    case 4: // Right (x+1)
      mesh.vertices.push_back({p101, color});
      mesh.vertices.push_back({p100, color});
      mesh.vertices.push_back({p110, color});
      mesh.vertices.push_back({p111, color});
      break;
    case 5: // Left (x)
      mesh.vertices.push_back({p000, color});
      mesh.vertices.push_back({p001, color});
      mesh.vertices.push_back({p011, color});
      mesh.vertices.push_back({p010, color});
      break;
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
        // Le dessous utilise la couleur Dirt pour l'herbe, sinon la couleur côté
        glm::vec3 botColor =
            (type == BlockType::Grass) ? BlockDatabase::Get(BlockType::Dirt).color
                                       : sideColor;

        if (chunk.getBlock(x, y, z + 1) == BlockType::Air)
          addFace(x, y, z, 0, sideColor);
        if (chunk.getBlock(x, y, z - 1) == BlockType::Air)
          addFace(x, y, z, 1, sideColor);
        if (chunk.getBlock(x, y + 1, z) == BlockType::Air)
          addFace(x, y, z, 2, topColor);
        if (chunk.getBlock(x, y - 1, z) == BlockType::Air)
          addFace(x, y, z, 3, botColor);
        if (chunk.getBlock(x + 1, y, z) == BlockType::Air)
          addFace(x, y, z, 4, sideColor);
        if (chunk.getBlock(x - 1, y, z) == BlockType::Air)
          addFace(x, y, z, 5, sideColor);
      }
    }
  }

  return mesh;
}
