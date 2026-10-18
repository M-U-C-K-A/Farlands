// -----------------------------------------------------------------------------
// Fichier : leaf_particles.h
// Rôle : Feuilles et pétales qui tombent des arbres autour du joueur.
// Simulation CPU légère (quelques centaines de particules) ; le rendu est fait
// par le Renderer avec des quads instanciés.
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <random>
#include <vector>

class World;

/// Donnée par instance envoyée au GPU (32 octets, voir shaders/particle.vert)
struct ParticleInstance
{
	glm::vec3 rel;  // position relative à la caméra
	float size;     // côté du quad (blocs)
	uint32_t color; // RGBA8 sRGB
	float angle;    // rotation autour de la verticale
	float tilt;     // inclinaison (rotation autour de l'axe horizontal)
	float alpha;    // 0..1 (fondu par tramage)
};
static_assert(sizeof(ParticleInstance) == 32, "ParticleInstance must stay 32 bytes");

class LeafParticles
{
  public:
	static constexpr size_t MAX_PARTICLES = 1500;

	/// Fait apparaître, déplace et retire les particules
	void update(float dt, float time, const World &world, const glm::vec3 &camera,
				bool enabled);
	/// Instances relatives à la caméra pour le rendu
	void buildInstances(const glm::vec3 &camera, std::vector<ParticleInstance> &out) const;
	size_t count() const { return m_particles.size(); }

  private:
	struct Particle
	{
		glm::vec3 pos;
		float fallSpeed;
		float angle, spin;
		float tilt, tiltSpeed;
		float size;
		float age, life;
		float landedTime; // < 0 : en vol
		float seed;
		bool onWater;
		uint32_t color;
	};

	std::vector<Particle> m_particles;
	std::mt19937 m_rng{1234};
	float m_spawnBudget = 0.0f;

	float rand01() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(m_rng); }
};
