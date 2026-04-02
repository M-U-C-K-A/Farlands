#pragma once

#include <GLFW/glfw3.h>

#include "renderer/camera.h"
#include "renderer/renderer.h"
#include "ui/menu.h"
#include "world/world.h"

class Application
{
  public:
	void run();

  private:
	void initWindow();
	void mainLoop();
	void cleanup();

	static void
	framebufferResizeCallback(GLFWwindow *window, int width, int height);

	GLFWwindow *m_window = nullptr;
	Renderer m_renderer;
	Camera m_camera;
	MainMenu m_menu;
	World m_world;

	static constexpr int WIDTH = 1280;
	static constexpr int HEIGHT = 720;
};
