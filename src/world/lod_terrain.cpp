// -----------------------------------------------------------------------------
// Fichier : lod_terrain.cpp
// Rôle : Génération et gestion des tuiles du terrain lointain.
// -----------------------------------------------------------------------------
#include "lod_terrain.h"

#include <algorithm>
#include <cmath>

namespace
	{
		uint32_t rgb(int r, int g, int b)
		{
			auto c = [](int v) { return static_cast<uint32_t>(std::clamp(v, 0, 255)); };
			return c(r) | (c(g) << 8) | (c(b) << 16) | (255u << 24);
		}

		// Couleur sRGB × teinte de biome encodée sur [0, 2]
		uint32_t tinted(int r, int g, int b, uint32_t tint)
		{
			auto ch = [&](int v, int i) {
				return static_cast<int>(v * (((tint >> (i * 8)) & 0xFFu) / 127.5f));
			};
			return rgb(ch(r, 0), ch(g, 1), ch(b, 2));
		}

		uint32_t packNormal(const glm::vec3 &n, bool water)
		{
			auto s = [](float v) {
				return static_cast<uint32_t>(static_cast<int8_t>(std::clamp(v, -1.0f, 1.0f) * 127.0f))
					   & 0xFFu;
			};
			return s(n.x) | (s(n.y) << 8) | (s(n.z) << 16) | ((water ? 127u : 0u) << 24);
		}

		// Résolution selon la distance de la tuile à la caméra
		int stepForDistance(float d)
		{
			if(d < 700.0f)
				return 4;
			if(d < 1500.0f)
				return 8;
			return 16;
		}
	}

LodTerrain::LodTerrain(const TerrainGenerator &generator) : m_gen(generator)
{
	m_jobs = std::make_unique<JobSystem>(3);
}

LodTerrain::~LodTerrain() { m_jobs.reset(); }

void LodTerrain::update(const glm::vec3 &camera, int lodDistance, int chunkRadius)
{
	// ── Résultats des threads ───────────────────────────────────
	std::vector<std::shared_ptr<LodMesh>> done;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		done.swap(m_results);
	}
	for(auto &mesh : done)
		{
			TileKey key{mesh->tile.x, mesh->tile.y};
			auto p = m_pending.find(key);
			if(p != m_pending.end() && p->second == mesh->step)
				m_pending.erase(p);
			auto d = m_desired.find(key);
			if(d == m_desired.end() || d->second != mesh->step)
				continue; // devenue inutile entre-temps
			m_current[key] = mesh->step;
			m_events.push_back({mesh->tile, mesh});
		}

	// ── Nouvel ensemble de tuiles si la caméra a assez bougé ───
	glm::vec2 cam(camera.x, camera.z);
	if(glm::length(cam - m_center) < 64.0f && lodDistance == m_distance
	   && chunkRadius == m_chunkRadius)
		return;
	m_center = cam;
	m_distance = lodDistance;
	m_chunkRadius = chunkRadius;

	m_desired.clear();
	if(lodDistance > 0)
		{
			// Zone couverte par les vrais chunks : inutile d'y générer du LOD
			float inner = std::max(0.0f, (chunkRadius - 2.0f) * 16.0f);
			int r = lodDistance / TILE + 1;
			int ctx = static_cast<int>(std::floor(cam.x / TILE));
			int ctz = static_cast<int>(std::floor(cam.y / TILE));
			for(int tx = ctx - r; tx <= ctx + r; ++tx)
				for(int tz = ctz - r; tz <= ctz + r; ++tz)
					{
						glm::vec2 mn(tx * TILE, tz * TILE), mx = mn + glm::vec2(TILE);
						glm::vec2 nearest = glm::clamp(cam, mn, mx);
						float dNear = glm::length(nearest - cam);
						if(dNear > lodDistance)
							continue;
						// Point le plus éloigné : tuile entièrement dans les chunks ?
						glm::vec2 far(std::abs(cam.x - mn.x) > std::abs(cam.x - mx.x) ? mn.x : mx.x,
									  std::abs(cam.y - mn.y) > std::abs(cam.y - mx.y) ? mn.y : mx.y);
						if(glm::length(far - cam) < inner)
							continue;
						glm::vec2 center = (mn + mx) * 0.5f;
						m_desired[{tx, tz}] = stepForDistance(glm::length(center - cam));
					}
		}

	// Suppressions
	for(auto it = m_current.begin(); it != m_current.end();)
		{
			if(!m_desired.count(it->first))
				{
					m_events.push_back({glm::ivec2(it->first.x, it->first.z), nullptr});
					it = m_current.erase(it);
				}
			else
				++it;
		}

	// Constructions (les plus proches d'abord)
	for(auto &[key, step] : m_desired)
		{
			auto cur = m_current.find(key);
			if(cur != m_current.end() && cur->second == step)
				continue;
			auto pend = m_pending.find(key);
			if(pend != m_pending.end() && pend->second == step)
				continue;
			m_pending[key] = step;
			glm::vec2 center((key.x + 0.5f) * TILE, (key.z + 0.5f) * TILE);
			float prio = glm::length(center - cam);
			int tx = key.x, tz = key.z, st = step;
			m_jobs->submit(prio, [this, tx, tz, st] {
				auto mesh = buildTile(tx, tz, st);
				std::lock_guard<std::mutex> lock(m_mutex);
				m_results.push_back(std::move(mesh));
			});
		}
}

std::vector<LodTerrain::Event> LodTerrain::takeEvents()
{
	std::vector<Event> out;
	out.swap(m_events);
	return out;
}

std::shared_ptr<LodMesh> LodTerrain::buildTile(int tx, int tz, int step) const
{
	auto mesh = std::make_shared<LodMesh>();
	mesh->tile = glm::ivec2(tx, tz);
	mesh->step = step;

	const int N = TILE / step;     // cellules par côté
	const int G = N + 3;           // grille avec bordure (normales)
	const int ox = tx * TILE, oz = tz * TILE;

	// Hauteurs du terrain (bordure d'un pas pour les pentes et normales)
	std::vector<int> h(G * G);
	for(int j = 0; j < G; ++j)
		for(int i = 0; i < G; ++i)
			h[j * G + i] = m_gen.getHeightAt(ox + (i - 1) * step, oz + (j - 1) * step);
	auto H = [&](int i, int j) { return h[(j + 1) * G + (i + 1)]; };

	// Surface finale (canopée incluse) et couleur de chaque sommet
	std::vector<float> top((N + 1) * (N + 1));
	std::vector<uint32_t> color((N + 1) * (N + 1));
	std::vector<uint8_t> water((N + 1) * (N + 1));
	for(int j = 0; j <= N; ++j)
		for(int i = 0; i <= N; ++i)
			{
				int wx = ox + i * step, wz = oz + j * step;
				int hh = H(i, j);
				int idx = j * (N + 1) + i;
				if(hh <= SEA_LEVEL)
					{
						top[idx] = SEA_LEVEL - 0.125f;
						color[idx] = rgb(40, 90, 110);
						water[idx] = 1;
						continue;
					}
				water[idx] = 0;
				float temp = m_gen.getTemperature(wx, wz);
				float hum = m_gen.getHumidity(wx, wz);
				BiomeType biome = m_gen.biomeForHeight(hh, wx, wz);
				int slope = std::max(std::abs(H(i + 1, j) - H(i - 1, j)),
									 std::abs(H(i, j + 1) - H(i, j - 1)))
							/ std::max(1, step / 2);

				float y = static_cast<float>(hh);
				uint32_t c;
				switch(biome)
					{
					case BiomeType::Beach:
					case BiomeType::Desert: c = rgb(214, 202, 158); break;
					case BiomeType::SnowyPeaks:
					case BiomeType::SnowyTaiga: c = rgb(236, 240, 248); break;
					default: c = tinted(84, 116, 44, m_gen.grassTint(temp, hum)); break;
					}
				if(slope >= 4 && biome != BiomeType::Beach)
					c = rgb(118, 118, 122); // falaise
				else if(biome == BiomeType::SnowyPeaks && slope >= 3)
					c = rgb(150, 150, 156);

				// Canopée des forêts : surface relevée, couleur du feuillage
				float canopy = 0.0f;
				switch(biome)
					{
					case BiomeType::Forest:
						canopy = 5.0f;
						c = tinted(70, 128, 48, m_gen.foliageTint(temp, hum));
						break;
					case BiomeType::Taiga:
						canopy = 6.0f;
						c = rgb(44, 82, 56);
						break;
					case BiomeType::SnowyTaiga:
						canopy = 5.0f;
						c = rgb(70, 98, 84);
						break;
					case BiomeType::CherryGrove:
						canopy = 4.0f;
						c = rgb(238, 168, 198);
						break;
					default: break;
					}
				if(slope >= 4)
					canopy = 0.0f;
				// Sous les vrais blocs (ils l'emportent dans la zone commune)
				top[idx] = y + canopy - 0.6f;
				color[idx] = c;
			}

	// Sommets (normales par différences centrées sur la surface finale)
	auto T = [&](int i, int j) {
		i = std::clamp(i, 0, N);
		j = std::clamp(j, 0, N);
		return top[j * (N + 1) + i];
	};
	mesh->vertices.reserve((N + 1) * (N + 1) + 4 * (N + 1));
	mesh->minY = 1e9f;
	mesh->maxY = -1e9f;
	for(int j = 0; j <= N; ++j)
		for(int i = 0; i <= N; ++i)
			{
				int idx = j * (N + 1) + i;
				glm::vec3 n = water[idx]
								? glm::vec3(0, 1, 0)
								: glm::normalize(glm::vec3(T(i - 1, j) - T(i + 1, j), 2.0f * step,
														   T(i, j - 1) - T(i, j + 1)));
				LodVertex v;
				v.pos = glm::vec3(i * step, top[idx], j * step);
				v.normal = packNormal(n, water[idx]);
				v.color = color[idx];
				mesh->vertices.push_back(v);
				mesh->minY = std::min(mesh->minY, v.pos.y);
				mesh->maxY = std::max(mesh->maxY, v.pos.y);
			}
	for(int j = 0; j < N; ++j)
		for(int i = 0; i < N; ++i)
			{
				uint32_t a = j * (N + 1) + i, b = a + 1, c = a + (N + 1), d = c + 1;
				mesh->indices.insert(mesh->indices.end(), {a, c, b, b, c, d});
			}

	// Jupes : bandes verticales sur les bords (masquent les fentes entre
	// tuiles de résolutions différentes)
	const float skirt = 6.0f + step * 1.5f;
	auto addSkirt = [&](auto edgeIndex) {
		uint32_t first = static_cast<uint32_t>(mesh->vertices.size());
		for(int k = 0; k <= N; ++k)
			{
				LodVertex v = mesh->vertices[edgeIndex(k)];
				v.pos.y -= skirt;
				mesh->vertices.push_back(v);
			}
		for(int k = 0; k < N; ++k)
			{
				uint32_t a = edgeIndex(k), b = edgeIndex(k + 1);
				uint32_t c = first + k, d = first + k + 1;
				mesh->indices.insert(mesh->indices.end(), {a, b, c, b, d, c});
			}
	};
	addSkirt([&](int k) { return static_cast<uint32_t>(k); });
	addSkirt([&](int k) { return static_cast<uint32_t>(N * (N + 1) + k); });
	addSkirt([&](int k) { return static_cast<uint32_t>(k * (N + 1)); });
	addSkirt([&](int k) { return static_cast<uint32_t>(k * (N + 1) + N); });
	mesh->minY -= skirt;
	return mesh;
}
