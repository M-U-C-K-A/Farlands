#pragma once

#include <GLFW/glfw3.h>
#include "renderer.h"
#include "camera.h"

class Application {
public:
    void run();

private:
    void initWindow();
    void mainLoop();
    void cleanup();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* m_window = nullptr;
    Renderer m_renderer;
    Camera m_camera;

    static constexpr int WIDTH = 1280;
    static constexpr int HEIGHT = 720;
};
