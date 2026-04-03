// -----------------------------------------------------------------------------
// Fichier : app.h
// Rôle : Classe principale de l'application.
// Coordonne la fenêtre GLFW, le moteur de rendu, la caméra, l'UI et le monde.
// -----------------------------------------------------------------------------
#pragma once

#include <GLFW/glfw3.h>

#include "renderer/camera.h"
#include "renderer/renderer.h"
#include "ui/menu.h"
#include "world/world.h"

// -----------------------------------------------------------------------------
// Fichier : app.h
// Rôle : Définition de la classe Application qui orchestre tout le jeu.
// -----------------------------------------------------------------------------
class Application
{
  public:
    // Lance l'application de bout en bout (init, boucle principale, nettoyage)
	void run();

  private:
    // Initialise la fenêtre GLFW
	void initWindow();
    // Boucle principale de rendu et de mise à jour physique
	void mainLoop();
    // Nettoie les ressources Vulkan et la fenêtre
	void cleanup();

    // Callback invoqué par GLFW lorsque la fenêtre est redimensionnée
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
