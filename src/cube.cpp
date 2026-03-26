#include "cube.h"

// Cube vertices: 24 vertices (4 per face) with distinct colors per face
// Each face gets its own 4 vertices so colors aren't shared across faces
static const std::vector<Vertex> vertices = {
    // Front face (green)
    {{-0.5f, -0.5f,  0.5f}, {0.18f, 0.80f, 0.44f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.18f, 0.80f, 0.44f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.18f, 0.80f, 0.44f}},
    {{-0.5f,  0.5f,  0.5f}, {0.18f, 0.80f, 0.44f}},

    // Back face (blue)
    {{ 0.5f, -0.5f, -0.5f}, {0.20f, 0.60f, 0.86f}},
    {{-0.5f, -0.5f, -0.5f}, {0.20f, 0.60f, 0.86f}},
    {{-0.5f,  0.5f, -0.5f}, {0.20f, 0.60f, 0.86f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.20f, 0.60f, 0.86f}},

    // Top face (red/coral)
    {{-0.5f,  0.5f,  0.5f}, {0.91f, 0.30f, 0.24f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.91f, 0.30f, 0.24f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.91f, 0.30f, 0.24f}},
    {{-0.5f,  0.5f, -0.5f}, {0.91f, 0.30f, 0.24f}},

    // Bottom face (yellow)
    {{-0.5f, -0.5f, -0.5f}, {0.95f, 0.77f, 0.06f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.95f, 0.77f, 0.06f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.95f, 0.77f, 0.06f}},
    {{-0.5f, -0.5f,  0.5f}, {0.95f, 0.77f, 0.06f}},

    // Right face (purple)
    {{ 0.5f, -0.5f,  0.5f}, {0.56f, 0.27f, 0.68f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.56f, 0.27f, 0.68f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.56f, 0.27f, 0.68f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.56f, 0.27f, 0.68f}},

    // Left face (orange)
    {{-0.5f, -0.5f, -0.5f}, {0.90f, 0.49f, 0.13f}},
    {{-0.5f, -0.5f,  0.5f}, {0.90f, 0.49f, 0.13f}},
    {{-0.5f,  0.5f,  0.5f}, {0.90f, 0.49f, 0.13f}},
    {{-0.5f,  0.5f, -0.5f}, {0.90f, 0.49f, 0.13f}},
};

// 6 faces × 2 triangles × 3 indices = 36 indices
static const std::vector<uint16_t> indices = {
    // Front
     0,  1,  2,   2,  3,  0,
    // Back
     4,  5,  6,   6,  7,  4,
    // Top
     8,  9, 10,  10, 11,  8,
    // Bottom
    12, 13, 14,  14, 15, 12,
    // Right
    16, 17, 18,  18, 19, 16,
    // Left
    20, 21, 22,  22, 23, 20,
};

const std::vector<Vertex>& Cube::getVertices() {
    return vertices;
}

const std::vector<uint16_t>& Cube::getIndices() {
    return indices;
}
