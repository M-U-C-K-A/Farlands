#include "chunk.h"

/*
 * @brief Constructeur du chunk.
 * @brief Remplit le tableau de blocs avec de l'air.
 */
Chunk::Chunk() { m_blocks.fill(BlockType::Air); }

/*
 * @brief Calcule l'index du bloc dans le tableau 1D.
 * @brief Utilise l'algorithme de face culling pour optimiser le rendu.
 */
int Chunk::getIndex(int x, int y, int z) const {
  return x + CHUNK_SIZE_X * (y + CHUNK_SIZE_Y * z);
}

/*
 * @brief Définit le type de bloc à une position donnée.
 * @brief Vérifie que les coordonnées sont dans les limites du chunk.
 */
void Chunk::setBlock(int x, int y, int z, BlockType type) {
  if (x >= 0 && x < CHUNK_SIZE_X && y >= 0 && y < CHUNK_SIZE_Y && z >= 0 &&
      z < CHUNK_SIZE_Z) {
    m_blocks[getIndex(x, y, z)] = type;
  }
}

/*
 * @brief Récupère le type de bloc à une position donnée.
 * @brief Utilise l'algorithme de face culling pour optimiser le rendu.
 */
BlockType Chunk::getBlock(int x, int y, int z) const {
  if (x >= 0 && x < CHUNK_SIZE_X && y >= 0 && y < CHUNK_SIZE_Y && z >= 0 &&
      z < CHUNK_SIZE_Z) {
    return m_blocks[getIndex(x, y, z)];
  }
  return BlockType::Air;
}

/*
 * @brief Génère le maillage du chunk.
 * @brief Utilise l'algorithme de face culling pour optimiser le rendu.
 */
void Chunk::generateMesh() {
  m_vertices.clear();
  m_indices.clear();

  auto addFace = [&](int x, int y, int z, int faceType, glm::vec3 color) {
    uint32_t idx = static_cast<uint32_t>(m_vertices.size());
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
      m_vertices.push_back({p001, color});
      m_vertices.push_back({p101, color});
      m_vertices.push_back({p111, color});
      m_vertices.push_back({p011, color});
      break;
    case 1: // Back (z)
      m_vertices.push_back({p100, color});
      m_vertices.push_back({p000, color});
      m_vertices.push_back({p010, color});
      m_vertices.push_back({p110, color});
      break;
    case 2: // Top (y+1)
      m_vertices.push_back({p011, color});
      m_vertices.push_back({p111, color});
      m_vertices.push_back({p110, color});
      m_vertices.push_back({p010, color});
      break;
    case 3: // Bottom (y)
      m_vertices.push_back({p000, color});
      m_vertices.push_back({p100, color});
      m_vertices.push_back({p101, color});
      m_vertices.push_back({p001, color});
      break;
    case 4: // Right (x+1)
      m_vertices.push_back({p101, color});
      m_vertices.push_back({p100, color});
      m_vertices.push_back({p110, color});
      m_vertices.push_back({p111, color});
      break;
    case 5: // Left (x)
      m_vertices.push_back({p000, color});
      m_vertices.push_back({p001, color});
      m_vertices.push_back({p011, color});
      m_vertices.push_back({p010, color});
      break;
    }

    m_indices.push_back(idx + 0);
    m_indices.push_back(idx + 1);
    m_indices.push_back(idx + 2);
    m_indices.push_back(idx + 2);
    m_indices.push_back(idx + 3);
    m_indices.push_back(idx + 0);
  };

  glm::vec3 colorGrassFront(0.36f, 0.58f, 0.28f);
  glm::vec3 colorGrassTop(0.46f, 0.72f, 0.31f);
  glm::vec3 colorDirt(0.53f, 0.38f, 0.28f);
  glm::vec3 colorStone(0.5f, 0.5f, 0.5f);

  for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
    for (int y = 0; y < CHUNK_SIZE_Y; ++y) {
      for (int x = 0; x < CHUNK_SIZE_X; ++x) {
        BlockType type = getBlock(x, y, z);
        if (type == BlockType::Air)
          continue;

        glm::vec3 color = colorDirt;
        if (type == BlockType::Grass)
          color = colorGrassFront;
        else if (type == BlockType::Stone)
          color = colorStone;

        if (getBlock(x, y, z + 1) == BlockType::Air)
          addFace(x, y, z, 0, color);
        if (getBlock(x, y, z - 1) == BlockType::Air)
          addFace(x, y, z, 1, color);
        if (getBlock(x, y + 1, z) == BlockType::Air) {
          glm::vec3 topColor =
              (type == BlockType::Grass) ? colorGrassTop : color;
          addFace(x, y, z, 2, topColor);
        }
        if (getBlock(x, y - 1, z) == BlockType::Air) {
          glm::vec3 botColor = (type == BlockType::Grass) ? colorDirt : color;
          addFace(x, y, z, 3, botColor);
        }
        if (getBlock(x + 1, y, z) == BlockType::Air)
          addFace(x, y, z, 4, color);
        if (getBlock(x - 1, y, z) == BlockType::Air)
          addFace(x, y, z, 5, color);
      }
    }
  }
}
