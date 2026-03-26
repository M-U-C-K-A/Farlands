#include "app.h"
#include "cube.h"

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
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f),
                            glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view = m_camera.getViewMatrix();
    ubo.proj = m_camera.getProjectionMatrix();

    m_renderer.drawFrame(ubo);
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
  mainLoop();
  cleanup();
}
