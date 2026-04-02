#include "world.h"
#include "block.h"

#include <cmath>
#include <iostream>

// ── Generate ────────────────────────────────────────────────────
void World::generate(int radius) {
  m_radius = radius;
  m_chunks.clear();

  for (int cx = -radius; cx <= radius; ++cx) {
    for (int cz = -radius; cz <= radius; ++cz) {
      auto chunk = std::make_unique<Chunk>(cx, cz);
      generateTerrain(*chunk);
      m_chunks[glm::ivec2(cx, cz)] = std::move(chunk);
    }
  }

  int count = static_cast<int>(m_chunks.size());
  std::cout << "[World] Generated " << count << " chunks (radius=" << radius
            << ")" << std::endl;
}

// ── Terrain Generation ──────────────────────────────────────────
void World::generateTerrain(Chunk &chunk) {
  auto pos = chunk.getChunkPos();

  for (int x = 0; x < CHUNK_SIZE_X; ++x) {
    for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
      // World-space coordinates
      float wx = static_cast<float>(pos.x * CHUNK_SIZE_X + x);
      float wz = static_cast<float>(pos.y * CHUNK_SIZE_Z + z);

      // Simple height map using sin/cos for hills
      float h1 = std::sin(wx * 0.05f) * 4.0f;
      float h2 = std::cos(wz * 0.07f) * 3.0f;
      float h3 = std::sin((wx + wz) * 0.03f) * 5.0f;
      int height = 15 + static_cast<int>(h1 + h2 + h3);

      if (height < 1)
        height = 1;
      if (height > 60)
        height = 60;

      for (int y = 0; y < height; ++y) {
        if (y == 0) {
          chunk.setBlock(x, y, z, BlockType::Bedrock);
        } else if (y < height - 4) {
          chunk.setBlock(x, y, z, BlockType::Stone);
        } else if (y < height - 1) {
          chunk.setBlock(x, y, z, BlockType::Dirt);
        } else {
          chunk.setBlock(x, y, z, BlockType::Grass);
        }
      }
    }
  }
}

// ── Chunk Access ────────────────────────────────────────────────
Chunk *World::getChunk(int cx, int cz) {
  auto it = m_chunks.find(glm::ivec2(cx, cz));
  return it != m_chunks.end() ? it->second.get() : nullptr;
}

const Chunk *World::getChunk(int cx, int cz) const {
  auto it = m_chunks.find(glm::ivec2(cx, cz));
  return it != m_chunks.end() ? it->second.get() : nullptr;
}

// ── Build World Mesh ────────────────────────────────────────────
void World::buildWorldMesh(std::vector<Vertex> &outVertices,
                           std::vector<uint32_t> &outIndices) {
  outVertices.clear();
  outIndices.clear();

  for (auto &[pos, chunk] : m_chunks) {
    // Offset in world coordinates
    float offsetX = static_cast<float>(pos.x * CHUNK_SIZE_X);
    float offsetZ = static_cast<float>(pos.y * CHUNK_SIZE_Z);

    // Generate chunk mesh
    ChunkMesh mesh = generateChunkMesh(*chunk);

    // Offset vertices to world space
    uint32_t baseIndex = static_cast<uint32_t>(outVertices.size());
    for (auto &v : mesh.vertices) {
      v.pos.x += offsetX;
      v.pos.z += offsetZ;
      outVertices.push_back(v);
    }
    for (auto idx : mesh.indices) {
      outIndices.push_back(baseIndex + idx);
    }

    // Add chunk border lines
    addChunkBorders(outVertices, outIndices, pos.x, pos.y, 20);
  }

  std::cout << "[World] Built world mesh: " << outVertices.size()
            << " vertices, " << outIndices.size() << " indices" << std::endl;
}

// ── Chunk Border Visualization ──────────────────────────────────
void World::addChunkBorders(std::vector<Vertex> &vertices,
                            std::vector<uint32_t> &indices, int cx, int cz,
                            int terrainHeight) {
  float x0 = static_cast<float>(cx * CHUNK_SIZE_X);
  float z0 = static_cast<float>(cz * CHUNK_SIZE_Z);
  float x1 = x0 + CHUNK_SIZE_X;
  float z1 = z0 + CHUNK_SIZE_Z;

  // Couleur des bordures : jaune vif pour être bien visible
  glm::vec3 borderColor(1.0f, 0.85f, 0.0f);
  float borderWidth = 0.05f;

  // Hauteur des piliers de bordure
  float maxY = static_cast<float>(terrainHeight + 5);

  // Dessiner 4 piliers fins aux coins du chunk
  auto addPillar = [&](float px, float pz) {
    uint32_t base = static_cast<uint32_t>(vertices.size());
    float bw = borderWidth;

    // 8 sommets d'un pilier fin
    vertices.push_back({{px - bw, 0, pz - bw}, borderColor});
    vertices.push_back({{px + bw, 0, pz - bw}, borderColor});
    vertices.push_back({{px + bw, 0, pz + bw}, borderColor});
    vertices.push_back({{px - bw, 0, pz + bw}, borderColor});
    vertices.push_back({{px - bw, maxY, pz - bw}, borderColor});
    vertices.push_back({{px + bw, maxY, pz - bw}, borderColor});
    vertices.push_back({{px + bw, maxY, pz + bw}, borderColor});
    vertices.push_back({{px - bw, maxY, pz + bw}, borderColor});

    // 6 faces × 2 triangles
    // Front
    indices.insert(indices.end(),
                   {base + 0, base + 1, base + 5, base + 5, base + 4, base + 0,
                    // Back
                    base + 2, base + 3, base + 7, base + 7, base + 6, base + 2,
                    // Left
                    base + 3, base + 0, base + 4, base + 4, base + 7, base + 3,
                    // Right
                    base + 1, base + 2, base + 6, base + 6, base + 5, base + 1,
                    // Top
                    base + 4, base + 5, base + 6, base + 6, base + 7, base + 4,
                    // Bottom
                    base + 3, base + 2, base + 1, base + 1, base + 0,
                    base + 3});
  };

  addPillar(x0, z0);
  addPillar(x1, z0);
  addPillar(x0, z1);
  addPillar(x1, z1);

  // Barre horizontale en haut reliant les piliers (4 barres)
  auto addBar = [&](float ax, float az, float bx, float bz) {
    uint32_t base = static_cast<uint32_t>(vertices.size());
    float bw = borderWidth;
    float barY = maxY - bw;

    vertices.push_back({{ax - bw, barY, az - bw}, borderColor});
    vertices.push_back({{bx + bw, barY, bz - bw}, borderColor});
    vertices.push_back({{bx + bw, barY, bz + bw}, borderColor});
    vertices.push_back({{ax - bw, barY, az + bw}, borderColor});
    vertices.push_back({{ax - bw, maxY, az - bw}, borderColor});
    vertices.push_back({{bx + bw, maxY, bz - bw}, borderColor});
    vertices.push_back({{bx + bw, maxY, bz + bw}, borderColor});
    vertices.push_back({{ax - bw, maxY, az + bw}, borderColor});

    indices.insert(indices.end(),
                   {base + 0, base + 1, base + 5, base + 5, base + 4, base + 0,
                    base + 2, base + 3, base + 7, base + 7, base + 6, base + 2,
                    base + 3, base + 0, base + 4, base + 4, base + 7, base + 3,
                    base + 1, base + 2, base + 6, base + 6, base + 5, base + 1,
                    base + 4, base + 5, base + 6, base + 6, base + 7, base + 4,
                    base + 3, base + 2, base + 1, base + 1, base + 0,
                    base + 3});
  };

  // 4 barres en haut connectant les coins du chunk
  addBar(x0, z0, x1, z0); // North edge
  addBar(x0, z1, x1, z1); // South edge
  // For east/west edges, swap x/z logic
  auto addBarZ = [&](float ax, float az, float bx, float bz) {
    uint32_t base = static_cast<uint32_t>(vertices.size());
    float bw = borderWidth;
    float barY = maxY - bw;

    vertices.push_back({{ax - bw, barY, az - bw}, borderColor});
    vertices.push_back({{bx - bw, barY, bz + bw}, borderColor});
    vertices.push_back({{bx + bw, barY, bz + bw}, borderColor});
    vertices.push_back({{ax + bw, barY, az - bw}, borderColor});
    vertices.push_back({{ax - bw, maxY, az - bw}, borderColor});
    vertices.push_back({{bx - bw, maxY, bz + bw}, borderColor});
    vertices.push_back({{bx + bw, maxY, bz + bw}, borderColor});
    vertices.push_back({{ax + bw, maxY, az - bw}, borderColor});

    indices.insert(indices.end(),
                   {base + 0, base + 1, base + 5, base + 5, base + 4, base + 0,
                    base + 2, base + 3, base + 7, base + 7, base + 6, base + 2,
                    base + 3, base + 0, base + 4, base + 4, base + 7, base + 3,
                    base + 1, base + 2, base + 6, base + 6, base + 5, base + 1,
                    base + 4, base + 5, base + 6, base + 6, base + 7, base + 4,
                    base + 3, base + 2, base + 1, base + 1, base + 0,
                    base + 3});
  };

  addBarZ(x0, z0, x0, z1); // West edge
  addBarZ(x1, z0, x1, z1); // East edge
}
