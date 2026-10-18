// -----------------------------------------------------------------------------
// Fichier : app.h
// Rôle : Classe principale de l'application.
// Coordonne la fenêtre GLFW, le moteur de rendu, la caméra, l'UI et le monde.
// -----------------------------------------------------------------------------
#pragma once

#include <GLFW/glfw3.h>

#include <array>
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

	// ── Monde ───────────────────────────────────────────────────
	/// Crée (ou recrée) le monde : graine, type amplifié
	void createWorld(uint32_t seed, bool amplified);
	/// Téléporte (en vol) devant la structure la plus proche du type donné
	void teleportToStructure(StructureType type, float distanceScale = 1.0f);
	/// Téléporte vers le biome le plus proche (Amplified Peaks, cerisiers...)
	void teleportToBiome(BiomeType biome);
	/// Place la caméra (en vol) pour regarder un point depuis une distance
	void flyToView(const glm::vec3 &target, float distance, float height);
	void handleWorldRequest(const WorldRequest &req);
	/// Notification brève en bas de l'écran (vol, téléportation...)
	void notify(const std::string &text);
	void drawNotification();

	// ── Scripts de capture (FARLANDS_SCRIPT=fichier) ────────────
	// Une commande par ligne : wait, waitload, play, menu, tp, look,
	// preset, set, fly, shot, quit... (voir app_script.cpp)
	void loadScript(const std::string &path);
	void updateScript(float time);

	static void
	framebufferResizeCallback(GLFWwindow *window, int width, int height);
	static void
	cursorPosCallback(GLFWwindow *window, double xpos, double ypos);
	static void
	contentScaleCallback(GLFWwindow *window, float xscale, float yscale);
	static void scrollCallback(GLFWwindow *window, double xoff, double yoff);

	// ── Construction : casser (clic gauche), poser (clic droit) ──
	struct RayHit
	{
		bool hit = false;
		glm::ivec3 block{0};
		glm::ivec3 normal{0}; // face touchée
		BlockType type = BlockType::Air;
	};
	RayHit raycastBlock(float maxDist) const;
	void updateBuilding(float time);
	void drawHotbar();
	void drawBlockOutline(const RayHit &hit);

	GLFWwindow *m_window = nullptr;
	RenderSettings m_settings;
	Renderer m_renderer;
	Camera m_camera;
	MainMenu m_menu;
	std::unique_ptr<World> m_world;
	std::unique_ptr<LodTerrain> m_lod;
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

	// Construction
	std::array<BlockType, 9> m_hotbar{BlockType::Stone_Bricks, BlockType::Plank,
									  BlockType::Cobblestone,  BlockType::Glass,
									  BlockType::Log,          BlockType::White_Plaster,
									  BlockType::Lantern,      BlockType::Torch,
									  BlockType::Grass};
	int m_hotbarSlot = 0;
	RayHit m_target;
	float m_nextBreak = 0.0f, m_nextPlace = 0.0f;
	bool m_lmbWas = false, m_rmbWas = false, m_mmbWas = false;
	double m_scrollAccum = 0.0;
	bool m_hideHud = false; // F1

	// Notification
	std::string m_toast;
	float m_toastTime = -100.0f;
	float m_time = 0.0f;
	float m_fovScale = 1.0f; // sprint / zoom (lissé)

	// Script de capture
	std::vector<std::vector<std::string>> m_script;
	size_t m_scriptPos = 0;
	float m_scriptWaitUntil = 0.0f;
	bool m_scriptWaitLoad = false;
	float m_scriptLoadDeadline = 0.0f;
	std::string m_scriptDir;

	static constexpr int WIDTH = 1280;
	static constexpr int HEIGHT = 720;
};
