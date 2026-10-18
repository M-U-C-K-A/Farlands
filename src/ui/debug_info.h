// -----------------------------------------------------------------------------
// Fichier : debug_info.h
// Rôle : Données de performance et d'état affichées par l'interface (onglets
// Performance / Système des options, compteur de FPS). Remplies chaque frame
// par l'application.
// -----------------------------------------------------------------------------
#pragma once

#include "renderer/renderer.h"

#include <array>
#include <string>

struct DebugInfo
{
	// ── Temps de frame (historique circulaire, en ms) ───────────
	static constexpr int HISTORY = 300;
	std::array<float, HISTORY> frameMs{};
	int historyHead = 0; // prochaine écriture
	float fps = 0.0f;         // moyenne glissante
	float frameAvgMs = 0.0f;  // moyenne sur l'historique
	float frameMaxMs = 0.0f;  // pire frame de l'historique
	float low1Fps = 0.0f;     // "1% low" : FPS du percentile 99 des temps

	// ── CPU (ms, moyennes glissantes) ───────────────────────────
	float worldUpdateMs = 0.0f;
	float uploadMs = 0.0f;
	float uiMs = 0.0f;

	// ── Monde ───────────────────────────────────────────────────
	int chunksLoaded = 0;
	int chunksMeshed = 0;
	size_t pendingJobs = 0;
	unsigned workerThreads = 0;
	int renderRadius = 0;
	int lodTiles = 0;
	int particles = 0;

	// ── Joueur ──────────────────────────────────────────────────
	glm::vec3 position{0.0f};
	std::string biome;

	// ── Rendu ───────────────────────────────────────────────────
	Renderer::Stats render;
	Renderer::DeviceInfo device;

	// ── Système ─────────────────────────────────────────────────
	unsigned cpuThreads = 0;
	int windowW = 0, windowH = 0;
	int monitorW = 0, monitorH = 0, monitorHz = 0;
	bool fullscreen = false;
	float uptime = 0.0f;

	void pushFrame(float ms);
};
