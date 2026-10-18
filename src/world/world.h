// -----------------------------------------------------------------------------
// Fichier : world.h
// Rôle : Monde infini composé de chunks chargés dynamiquement autour du joueur.
// La génération et le maillage tournent sur un pool de threads ; le thread
// principal publie les chunks prêts et transmet les meshes au renderer.
// -----------------------------------------------------------------------------
#pragma once

#include "../core/job_system.h"
#include "chunk.h"
#include "chunk_mesh.h"
#include "terrain_generator.h"

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/// Hash pour glm::ivec2 (position chunk)
struct IVec2Hash
{
	std::size_t operator()(const glm::ivec2 &v) const
	{
		uint64_t k = (static_cast<uint64_t>(static_cast<uint32_t>(v.x)) << 32)
					 | static_cast<uint32_t>(v.y);
		k ^= k >> 33;
		k *= 0xff51afd7ed558ccdull;
		k ^= k >> 33;
		return static_cast<std::size_t>(k);
	}
};

class World
{
  public:
	explicit World(uint32_t seed = 42, bool amplified = false);
	~World();

	/// Évènement pour le renderer : nouveau mesh, ou suppression si mesh == null
	struct MeshEvent
	{
		glm::ivec2 pos;
		std::shared_ptr<ChunkMeshData> mesh;
	};

	/// Configure le répertoire de sauvegarde des chunks modifiés
	void setWorldDir(const std::string &dir);

	/// Rayon de rendu en chunks
	void setRenderRadius(int radius);
	int getRenderRadius() const { return m_radius; }

	/// À appeler à chaque frame (thread principal)
	void update(const glm::vec3 &playerPos);

	/// Récupère les évènements de mesh (au plus maxUploads nouveaux meshes)
	std::vector<MeshEvent> takeMeshEvents(size_t maxUploads);

	/// Récupère un chunk chargé par sa position, ou nullptr
	const Chunk *getChunk(int cx, int cz) const;

	/// Retourne le type de bloc aux coordonnées monde absolues
	BlockType getBlockAt(int wx, int wy, int wz) const;
	/// Le chunk contenant la colonne (wx, wz) est-il chargé ?
	bool isColumnLoaded(int wx, int wz) const;

	/// Modifie un bloc (le joueur casse / pose). Le chunk est copié s'il est
	/// partagé avec un maillage en cours, marqué modifié (sauvegardé) ; les
	/// remaillages (chunk et voisins : lumière, occlusion) sont regroupés et
	/// lancés en priorité au prochain update().
	bool setBlockAt(int wx, int wy, int wz, BlockType type);

	int getChunkCount() const { return static_cast<int>(m_chunks.size()); }
	int getMeshedCount() const { return m_meshedCount; }
	size_t getPendingJobs() const { return m_jobs->pending(); }
	unsigned getWorkerCount() const { return m_jobs->threadCount(); }

	const TerrainGenerator &getGenerator() const { return m_generator; }

	/// Sauvegarde les chunks modifiés (à la fermeture)
	void saveAll();

  private:
	enum class MeshState : uint8_t
	{
		None,
		Pending,
		Done
	};

	TerrainGenerator m_generator;
	std::string m_worldDir;
	int m_radius = 20;
	glm::ivec2 m_center{0};
	bool m_centerValid = false;
	int m_meshedCount = 0;

	// ── État du thread principal ────────────────────────────────
	std::unordered_map<glm::ivec2, std::shared_ptr<Chunk>, IVec2Hash> m_chunks;
	std::unordered_set<glm::ivec2, IVec2Hash> m_genPending;
	std::unordered_map<glm::ivec2, MeshState, IVec2Hash> m_meshState;
	std::unordered_map<glm::ivec2, uint32_t, IVec2Hash> m_meshVersion;
	std::unordered_set<glm::ivec2, IVec2Hash> m_edited; // à remailler
	std::deque<MeshEvent> m_events;

	// ── Résultats des threads de travail ────────────────────────
	std::mutex m_resultMutex;
	std::vector<std::shared_ptr<Chunk>> m_genResults;
	std::vector<glm::ivec2> m_genCancelled;
	std::vector<std::shared_ptr<ChunkMeshData>> m_meshResults;

	// Centre/rayon courants, lus par les travaux pour s'annuler
	std::atomic<int> m_atomicCX{0};
	std::atomic<int> m_atomicCZ{0};
	std::atomic<int> m_atomicRadius{20};

	void scheduleAround();
	void unloadFar();
	void drainResults();
	void tryScheduleMesh(const glm::ivec2 &pos);
	void flushEdits();
	/// Lance le maillage (voisins requis) ; urgent : passe devant la file
	bool scheduleMesh(const glm::ivec2 &pos, bool urgent);
	bool inRadius(const glm::ivec2 &pos, float radius) const;

	// Déclaré en dernier : détruit (threads joints) avant le reste
	std::unique_ptr<JobSystem> m_jobs;
};
