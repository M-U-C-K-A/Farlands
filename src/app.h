// -----------------------------------------------------------------------------
// Fichier : app.h
// Rôle : Classe principale de l'application.
// Coordonne la fenêtre GLFW, le moteur de rendu, la caméra, l'UI et le monde.
// -----------------------------------------------------------------------------
#pragma once

#include <GLFW/glfw3.h>

#include <string>

#include "player/player.h"
#include "renderer/camera.h"
#include "renderer/render_settings.h"
#include "renderer/renderer.h"
#include "ui/debug_info.h"
#include "ui/menu.h"
#include "world/leaf_particles.h"
#include "world/lod_terrain.h"
#include "world/world.h"

class Application
{
  public:
	void run();

  private:
	void initWindow();
	void mainLoop();
	void cleanup();

	void setCursorCaptured(bool captured);
	/// Plein écran sans bordure (F11) / retour au fenêtré
	void toggleFullscreen();
	std::string makeScreenshotPath() const;
	/// Cherche un point d'apparition avec vue (lac devant, montagnes au loin)
	void findScenicSpawn();
	void updateCamera(float time);
	void drawHud(float deltaTime);
	void updateDebugInfo(float time);
	void onPresetChanged();

	static void
	framebufferResizeCallback(GLFWwindow *window, int width, int height);
	static void
	cursorPosCallback(GLFWwindow *window, double xpos, double ypos);
	static void
	contentScaleCallback(GLFWwindow *window, float xscale, float yscale);

	GLFWwindow *m_window = nullptr;
	RenderSettings m_settings;
	Renderer m_renderer;
	Camera m_camera;
	MainMenu m_menu;
	World m_world{42};
	LodTerrain m_lod{m_world.getGenerator()};
	Player m_player;
	LeafParticles m_leaves;
	std::vector<ParticleInstance> m_particleInstances;

	// État souris
	bool m_firstMouse = true;
	double m_lastMouseX = 0.0;
	double m_lastMouseY = 0.0;
	bool m_cursorCaptured = false;
	bool m_menuWasActive = true; // le menu est actif au démarrage

	// Partie
	bool m_gameStarted = false;  // le joueur a cliqué "Singleplayer"
	bool m_spawned = false;      // le joueur a été posé sur le terrain
	glm::vec3 m_spawnPos{0.0f};
	float m_spawnYaw = 0.0f;
	bool m_showDebug = false;    // overlay F3
	float m_spawnSunAzimuth = 0.0f; // soleil "de face" choisi au spawn
	DebugInfo m_debug;

	// Plein écran (F11) : position/taille du mode fenêtré à restaurer
	bool m_fullscreen = false;
	int m_windowedX = 100, m_windowedY = 100;
	int m_windowedW = 1280, m_windowedH = 720;

	std::string m_dataDir = ".farlands";

	static constexpr int WIDTH = 1280;
	static constexpr int HEIGHT = 720;
};
