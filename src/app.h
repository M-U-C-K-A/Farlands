// -----------------------------------------------------------------------------
// Fichier : app.h
// Rôle : Classe principale de l'application.
// Coordonne la fenêtre GLFW, le moteur de rendu, la caméra, l'UI et le monde.
// -----------------------------------------------------------------------------
#pragma once

#include <GLFW/glfw3.h>

#include "player/player.h"
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
	static void
	cursorPosCallback(GLFWwindow *window, double xpos, double ypos);

	GLFWwindow *m_window = nullptr;
	Renderer m_renderer;
	Camera m_camera;
	MainMenu m_menu;
	World m_world;
	Player m_player;

	// État souris
	bool m_firstMouse = true;
	double m_lastMouseX = 0.0;
	double m_lastMouseY = 0.0;
	bool m_cursorCaptured = false;
	bool m_menuWasActive = false;

	// Monde infini
	int m_lastPlayerChunkX = INT_MIN;
	int m_lastPlayerChunkZ = INT_MIN;
	static constexpr int RENDER_RADIUS = 5;

	static constexpr int WIDTH = 1280;
	static constexpr int HEIGHT = 720;
};
