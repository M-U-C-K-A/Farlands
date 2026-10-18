// -----------------------------------------------------------------------------
// Fichier : leaf_particles.cpp
// Rôle : Simulation des feuilles qui tombent (apparition sous les feuillages,
// chute balancée par le vent, atterrissage puis disparition en fondu).
// -----------------------------------------------------------------------------
#include "leaf_particles.h"

#include "block.h"
#include "world.h"

#include <algorithm>
#include <cmath>

namespace
	{
		uint32_t rgba(int r, int g, int b)
		{
			auto c = [](int v) { return static_cast<uint32_t>(std::clamp(v, 0, 255)); };
			return c(r) | (c(g) << 8) | (c(b) << 16) | (255u << 24);
		}

		// Multiplie une couleur sRGB par une teinte encodée sur [0, 2]
		uint32_t applyTint(uint32_t color, uint32_t tint)
		{
			uint32_t out = 255u << 24;
			for(int i = 0; i < 3; ++i)
				{
					float c = static_cast<float>((color >> (i * 8)) & 0xFFu);
					float t = static_cast<float>((tint >> (i * 8)) & 0xFFu) / 127.5f;
					out |= static_cast<uint32_t>(std::clamp(c * t, 0.0f, 255.0f)) << (i * 8);
				}
			return out;
		}

		constexpr float SPAWN_RADIUS = 26.0f;
		constexpr float DESPAWN_RADIUS = 44.0f;
	}

void LeafParticles::update(float dt, float time, const World &world, const glm::vec3 &camera,
						   bool enabled)
{
	// ── Apparition : sondage aléatoire de blocs de feuillage proches ──
	if(enabled)
		{
			m_spawnBudget += dt * 1600.0f; // tentatives par seconde
			int attempts = static_cast<int>(m_spawnBudget);
			m_spawnBudget -= static_cast<float>(attempts);
			for(int i = 0; i < attempts && m_particles.size() < MAX_PARTICLES; ++i)
				{
					float r = SPAWN_RADIUS * std::sqrt(rand01());
					float a = rand01() * 6.2831853f;
					glm::vec3 p(camera.x + std::cos(a) * r, camera.y - 6.0f + rand01() * 26.0f,
								camera.z + std::sin(a) * r);
					glm::ivec3 b(static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)),
								 static_cast<int>(std::floor(p.z)));
					BlockType t = world.getBlockAt(b.x, b.y, b.z);
					bool cherry = t == BlockType::Cherry_Leaves;
					bool broad = t == BlockType::Leaves || t == BlockType::Birch_Leaves;
					if(!cherry && !broad)
						continue;
					if(world.getBlockAt(b.x, b.y - 1, b.z) != BlockType::Air)
						continue;
					// Les cerisiers perdent plus de pétales que les autres arbres
					if(!cherry && rand01() > 0.35f)
						continue;

					Particle q;
					q.pos = glm::vec3(b.x + rand01(), b.y - 0.02f, b.z + rand01());
					q.fallSpeed = cherry ? 0.55f + rand01() * 0.45f : 0.85f + rand01() * 0.6f;
					q.angle = rand01() * 6.2831853f;
					q.spin = (rand01() - 0.5f) * 4.0f;
					q.tilt = rand01() * 6.2831853f;
					q.tiltSpeed = (cherry ? 2.5f : 1.6f) * (0.5f + rand01());
					q.size = cherry ? 0.24f + rand01() * 0.10f : 0.28f + rand01() * 0.12f;
					q.age = 0.0f;
					q.life = 40.0f;
					q.landedTime = -1.0f;
					q.seed = rand01() * 100.0f;
					q.onWater = false;
					if(cherry)
						q.color = rand01() < 0.5f ? rgba(255, 186, 206) : rgba(248, 160, 190);
					else
						{
							const Chunk *chunk = world.getChunk(floorDiv(b.x, CHUNK_SIZE_X),
																floorDiv(b.z, CHUNK_SIZE_Z));
							uint32_t tint = chunk ? chunk->getFoliageTint(b.x - floorDiv(b.x, CHUNK_SIZE_X) * CHUNK_SIZE_X,
																		  b.z - floorDiv(b.z, CHUNK_SIZE_Z) * CHUNK_SIZE_Z)
												  : 0xFF808080u;
							uint32_t base = t == BlockType::Birch_Leaves ? rgba(120, 165, 64) : rgba(78, 140, 52);
							// Quelques feuilles déjà jaunies
							if(rand01() < 0.18f)
								base = rgba(176, 150, 56);
							q.color = applyTint(base, tint);
						}
					m_particles.push_back(q);
				}
		}

	// ── Simulation ──────────────────────────────────────────────
	glm::vec2 wind(0.55f, 0.2f);
	for(Particle &q : m_particles)
		{
			q.age += dt;
			if(q.landedTime >= 0.0f)
				{
					q.landedTime += dt;
					if(q.onWater) // dérive lente à la surface
						q.pos += glm::vec3(wind.x, 0.0f, wind.y) * 0.25f * dt;
					continue;
				}
			float sway = std::sin(time * 1.3f + q.seed * 7.0f);
			float sway2 = std::cos(time * 0.9f + q.seed * 3.0f);
			glm::vec3 vel(wind.x + sway * 0.7f, -q.fallSpeed * (0.75f + 0.25f * sway2),
						  wind.y + sway2 * 0.6f);
			q.pos += vel * dt;
			q.angle += q.spin * dt;
			q.tilt += q.tiltSpeed * dt;

			glm::ivec3 b(static_cast<int>(std::floor(q.pos.x)), static_cast<int>(std::floor(q.pos.y)),
						 static_cast<int>(std::floor(q.pos.z)));
			BlockType t = world.getBlockAt(b.x, b.y, b.z);
			if(t == BlockType::Water)
				{
					q.pos.y = b.y + 0.875f + 0.01f;
					q.landedTime = 0.0f;
					q.onWater = true;
				}
			else if(BlockDatabase::IsOpaqueCube(t))
				{
					q.pos.y = b.y + 1.01f;
					q.landedTime = 0.0f;
				}
		}

	// ── Retrait ─────────────────────────────────────────────────
	m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
									 [&](const Particle &q) {
										 glm::vec2 d(q.pos.x - camera.x, q.pos.z - camera.z);
										 return q.age > q.life || q.landedTime > 9.0f
												|| glm::dot(d, d) > DESPAWN_RADIUS * DESPAWN_RADIUS
												|| q.pos.y < 0.0f;
									 }),
					  m_particles.end());
}

void LeafParticles::buildInstances(const glm::vec3 &camera,
								   std::vector<ParticleInstance> &out) const
{
	out.clear();
	out.reserve(m_particles.size());
	for(const Particle &q : m_particles)
		{
			ParticleInstance inst;
			inst.rel = q.pos - camera;
			inst.size = q.size;
			inst.color = q.color;
			inst.angle = q.angle;
			// Posée : à plat sur le sol
			inst.tilt = q.landedTime >= 0.0f ? 1.5707963f : q.tilt;
			float fadeIn = std::min(1.0f, q.age * 3.0f);
			float fadeOut = q.landedTime >= 0.0f ? std::clamp(1.0f - (q.landedTime - 6.0f) / 3.0f, 0.0f, 1.0f)
												 : 1.0f;
			inst.alpha = fadeIn * fadeOut;
			out.push_back(inst);
		}
}
