// -----------------------------------------------------------------------------
// Fichier : lod_terrain.h
// Rôle : Terrain lointain simplifié (LOD), au-delà de la distance de rendu des
// chunks. Des tuiles de 256 blocs sont générées directement depuis le
// générateur de terrain (sans chunks complets) sous forme de grilles de
// hauteur dont la résolution diminue avec la distance (4, 8 puis 16 blocs).
// Couleurs de surface par biome, neige, roche, eau, canopée des forêts.
// -----------------------------------------------------------------------------
#pragma once

#include "../core/job_system.h"
#include "terrain_generator.h"

#include <glm/glm.hpp>

#include <map>
#include <memory>
#include <mutex>
#include <vector>

/// Sommet du terrain lointain (20 octets, voir shaders/lod.vert)
struct LodVertex
{
	glm::vec3 pos;   // relatif à l'origine de la tuile (x, z), y absolu
	uint32_t normal; // SNORM8 x3 ; w : 127 = eau
	uint32_t color;  // RGBA8 sRGB
};
static_assert(sizeof(LodVertex) == 20, "LodVertex must stay 20 bytes");

struct LodMesh
{
	glm::ivec2 tile{0}; // coordonnées de tuile
	int step = 0;       // blocs entre deux sommets
	std::vector<LodVertex> vertices;
	std::vector<uint32_t> indices;
	float minY = 0.0f, maxY = 0.0f;
};

class LodTerrain
{
  public:
	static constexpr int TILE = 256;

	explicit LodTerrain(const TerrainGenerator &generator);
	~LodTerrain();

	/// Met à jour l'ensemble de tuiles voulu (thread principal)
	void update(const glm::vec3 &camera, int lodDistance, int chunkRadius);

	struct Event
	{
		glm::ivec2 tile;
		std::shared_ptr<LodMesh> mesh; // nullptr = suppression
	};
	std::vector<Event> takeEvents();

	int tileCount() const { return static_cast<int>(m_current.size()); }
	size_t pendingJobs() const { return m_jobs->pending(); }

  private:
	struct TileKey
	{
		int x, z;
		bool operator<(const TileKey &o) const { return x != o.x ? x < o.x : z < o.z; }
	};

	const TerrainGenerator &m_gen;
	glm::vec2 m_center{1e9f};
	int m_distance = -1;
	int m_chunkRadius = -1;

	std::map<TileKey, int> m_current; // tuiles envoyées au renderer -> pas
	std::map<TileKey, int> m_pending; // en construction -> pas
	std::map<TileKey, int> m_desired;

	std::mutex m_mutex;
	std::vector<std::shared_ptr<LodMesh>> m_results;
	std::vector<Event> m_events;

	std::shared_ptr<LodMesh> buildTile(int tx, int tz, int step) const;

	// Déclaré en dernier : threads arrêtés avant le reste
	std::unique_ptr<JobSystem> m_jobs;
};
