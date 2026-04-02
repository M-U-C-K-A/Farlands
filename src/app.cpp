#include "app.h"
#include "world/chunk.h"
#include "world/chunk_mesh.h"

#include <chrono>
#include <glm/gtc/matrix_transform.hpp>

void Application::framebufferResizeCallback(GLFWwindow *window, int, int) {
  auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
  app->m_renderer.framebufferResized = true;
}

void Application::initWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  m_window =
      glfwCreateWindow(WIDTH, HEIGHT, "Minecraft Vulkan", nullptr, nullptr);
  glfwSetWindowUserPointer(m_window, this);
  glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
}

void Application::mainLoop() {
  auto startTime = std::chrono::high_resolution_clock::now();

  while (!glfwWindowShouldClose(m_window)) {
    glfwPollEvents();

    // ESC to close
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(m_window, true);

    // Update MVP
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(currentTime - startTime).count();

    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    if (w > 0 && h > 0) {
      m_camera.setAspectRatio(static_cast<float>(w) / static_cast<float>(h));
    }

    UniformBufferObject ubo{};
    glm::mat4 model = glm::rotate(glm::mat4(1.0f),
                                  time * glm::radians(45.0f),
                                  glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::translate(
        model, glm::vec3(-CHUNK_SIZE_X / 2.0f, -8.0f, -CHUNK_SIZE_Z / 2.0f));
    ubo.model = model;
    ubo.view = m_camera.getViewMatrix();
    ubo.proj = m_camera.getProjectionMatrix();

    // ImGui frame: begin → draw UI → drawFrame finishes
    m_renderer.beginImGuiFrame();
    m_menu.draw(m_window);

    m_renderer.drawFrame(ubo, m_menu.isActive());
  }

  m_renderer.waitIdle();
}

void Application::cleanup() {
  m_renderer.cleanup();
  glfwDestroyWindow(m_window);
  glfwTerminate();
}

void Application::run() {
  initWindow();
  m_renderer.init(m_window);

  // Génération du terrain de test
  Chunk chunk(0, 0);
  for (int x = 0; x < CHUNK_SIZE_X; ++x) {
    for (int z = 0; z < CHUNK_SIZE_Z; ++z) {
      for (int y = 0; y < 16; ++y) {
        if (y < 12) {
          chunk.setBlock(x, y, z, BlockType::Stone);
        } else if (y < 15) {
          chunk.setBlock(x, y, z, BlockType::Dirt);
        } else {
          chunk.setBlock(x, y, z, BlockType::Grass);
        }
      }
    }
  }

  ChunkMesh mesh = generateChunkMesh(chunk);
  m_renderer.updateBuffers(mesh.vertices, mesh.indices);

  mainLoop();
  cleanup();
}
