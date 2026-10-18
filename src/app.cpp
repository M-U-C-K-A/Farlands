// -----------------------------------------------------------------------------
// Fichier : app.cpp
// Rôle : Implémentation du cœur de l'application (Application).
// Gère l'initialisation de GLFW, la boucle principale, le contrôleur joueur,
// le streaming du monde, l'interface (menu, options, overlay F3).
// -----------------------------------------------------------------------------
#include "app.h"
#include "core/logger.h"
#include "renderer/atmosphere.h"
#include "world/biome.h"
#include "world/block.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

// Callback pour ajuster le viewport Vulkan lors d'un resize de fenêtre GLFW
void Application::framebufferResizeCallback(GLFWwindow *window, int, int)
{
	auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	app->m_renderer.framebufferResized = true;
}

// Changement d'échelle (passage sur un écran Retina, plein écran)
void Application::contentScaleCallback(GLFWwindow *window, float, float)
{
	auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	app->m_renderer.framebufferResized = true;
}

// Plein écran sans bordure sur l'écran principal, ou retour au fenêtré
void Application::toggleFullscreen()
{
	if(!m_fullscreen)
		{
			glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
			glfwGetWindowSize(m_window, &m_windowedW, &m_windowedH);
			GLFWmonitor *monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode *mode = glfwGetVideoMode(monitor);
			glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height,
								 mode->refreshRate);
			m_fullscreen = true;
			LOG_NOTE("Fullscreen " << mode->width << "x" << mode->height << " @ "
								   << mode->refreshRate << " Hz");
		}
	else
		{
			glfwSetWindowMonitor(m_window, nullptr, m_windowedX, m_windowedY, m_windowedW,
								 m_windowedH, GLFW_DONT_CARE);
			m_fullscreen = false;
			LOG_NOTE("Windowed " << m_windowedW << "x" << m_windowedH);
		}
	m_renderer.framebufferResized = true;
	m_firstMouse = true;
}

// Molette : emplacement de la barre d'inventaire
void Application::scrollCallback(GLFWwindow *window, double, double yoff)
{
	auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	if(app->m_cursorCaptured && !app->m_menu.isActive())
		app->m_scrollAccum += yoff;
}

// Callback souris — route les mouvements vers le Player FPS
void Application::cursorPosCallback(GLFWwindow *window, double xpos, double ypos)
{
	auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));

	if(!app->m_cursorCaptured)
		return;

	if(app->m_firstMouse)
		{
			app->m_lastMouseX = xpos;
			app->m_lastMouseY = ypos;
			app->m_firstMouse = false;
			return;
		}

	double xOff = xpos - app->m_lastMouseX;
	double yOff = ypos - app->m_lastMouseY;
	app->m_lastMouseX = xpos;
	app->m_lastMouseY = ypos;

	app->m_player.handleMouseMove(xOff, yOff);
}

// Capture (mode jeu) ou libère (mode UI) le curseur souris
void Application::setCursorCaptured(bool captured)
{
	if(captured == m_cursorCaptured)
		return;
	m_cursorCaptured = captured;
	glfwSetInputMode(m_window, GLFW_CURSOR,
					 captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	if(captured)
		{
			m_firstMouse = true;
			LOG_NOTE("Cursor captured (Game mode)");
		}
	else
		{
			LOG_NOTE("Cursor released (UI mode)");
		}
}

// Chemin horodaté pour une capture d'écran (~/.farlands/screenshots)
std::string Application::makeScreenshotPath() const
{
	std::time_t t = std::time(nullptr);
	std::ostringstream oss;
	oss << m_dataDir << "/screenshots/"
		<< std::put_time(std::localtime(&t), "%Y-%m-%d_%H.%M.%S") << ".png";
	return oss.str();
}

// Initialise le contexte de la fenêtre GLFW
void Application::initWindow()
{
	LOG_INFO("Initializing GLFW window...");
	if(!glfwInit())
		throw std::runtime_error("Failed to initialize GLFW");
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(WIDTH, HEIGHT, "Farlands", nullptr, nullptr);
	if(!m_window)
		{
			LOG_ERROR("Failed to create GLFW window!");
			throw std::runtime_error("Failed to create GLFW window");
		}
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
	glfwSetCursorPosCallback(m_window, cursorPosCallback);
	glfwSetWindowContentScaleCallback(m_window, contentScaleCallback);
	glfwSetScrollCallback(m_window, scrollCallback);
	LOG_SUCCESS("Window initialized successfully.");
}

// ── Point d'apparition panoramique ──────────────────────────────
// Parcourt une grille autour de l'origine et note chaque position (et
// direction) : un lac juste devant, des sommets au loin, un sol herbeux.
void Application::findScenicSpawn()
{
	auto start = std::chrono::steady_clock::now();
	const TerrainGenerator &gen = m_world->getGenerator();
	float bestScore = -1e9f;
	glm::ivec2 bestPos(0);
	float bestAngle = 0.0f;

	const int RADIUS = 520, STEP = 26;
	for(int x = -RADIUS; x <= RADIUS; x += STEP)
		for(int z = -RADIUS; z <= RADIUS; z += STEP)
			{
				int h = gen.getHeightAt(x, z);
				if(h < SEA_LEVEL + 2 || h > SEA_LEVEL + 16)
					continue;
				BiomeType b = gen.getBiomeAt(x, z);
				if(b == BiomeType::Desert || b == BiomeType::Beach
				   || b == BiomeType::SnowyTaiga)
					continue;

				for(int d = 0; d < 16; ++d)
					{
						float a = d * (2.0f * 3.14159265f / 16.0f);
						float dx = std::cos(a), dz = std::sin(a);
						float eyeH = h + 2.0f;
						auto heightAt = [&](int dist) {
							return gen.getHeightAt(x + int(dx * dist), z + int(dz * dist));
						};
						// Lac au premier plan
						int water = 0;
						for(int s = 10; s <= 70; s += 10)
							if(heightAt(s) <= SEA_LEVEL)
								water++;
						// Ligne de vue : les sommets lointains doivent dépasser
						// l'horizon proche (angle d'élévation)
						float nearAngle = -1.0f;
						for(int s = 6; s <= 90; s += 7)
							nearAngle = std::max(nearAngle, (std::max(heightAt(s), SEA_LEVEL) - eyeH) / s);
						float farAngle = -1.0f;
						int maxH = 0;
						for(int s = 100; s <= 400; s += 25)
							{
								int hh = heightAt(s);
								maxH = std::max(maxH, hh);
								farAngle = std::max(farAngle, (hh - eyeH) / s);
							}
						if(heightAt(5) > h + 3)
							continue;
						float reveal = farAngle - std::max(nearAngle, 0.0f);

						float score = std::min(water, 5) * 1.2f
									  + std::clamp((maxH - 110) / 14.0f, 0.0f, 7.0f)
										  * std::clamp(reveal * 20.0f, 0.0f, 1.0f)
									  + std::clamp(reveal * 25.0f, 0.0f, 3.0f)
									  + (b == BiomeType::CherryGrove ? 1.0f : 0.0f)
									  - std::sqrt(float(x * x + z * z)) * 0.002f;
						if(water == 0)
							score -= 3.0f;
						// Vue dégagée : pas d'arbre sur le point ni devant
						auto clearView = [&] {
							if(gen.treeNear(x, z, 3))
								return false;
							for(int s = 4; s <= 28; s += 6)
								if(gen.treeNear(x + int(dx * s), z + int(dz * s), 3))
									return false;
							return true;
						};
						if(score > bestScore && clearView())
							{
								bestScore = score;
								bestPos = glm::ivec2(x, z);
								bestAngle = a;
							}
					}
			}

	m_spawnPos = glm::vec3(bestPos.x + 0.5f, gen.getHeightAt(bestPos.x, bestPos.y),
						   bestPos.y + 0.5f);
	m_spawnYaw = glm::degrees(bestAngle);
	m_player.setPosition(m_spawnPos + glm::vec3(0.0f, 2.0f, 0.0f));
	m_player.setLook(m_spawnYaw, 4.0f);

	// Soleil derrière le joueur, légèrement de côté : relief éclairé de face
	glm::vec3 front(std::cos(bestAngle), 0.0f, std::sin(bestAngle));
	float viewAzimuth = glm::degrees(std::atan2(front.x, front.z));
	m_settings.sunAzimuth = std::fmod(viewAzimuth + 145.0f + 360.0f, 360.0f);
	m_spawnSunAzimuth = m_settings.sunAzimuth;

	double ms = std::chrono::duration<double, std::milli>(
				  std::chrono::steady_clock::now() - start)
				  .count();
	LOG_INFO("Scenic spawn at (" << bestPos.x << ", " << m_spawnPos.y << ", " << bestPos.y
								 << "), yaw " << m_spawnYaw << ", score " << bestScore
								 << " (" << ms << " ms)");
}

// ── Monde : création, exploration ───────────────────────────────
void Application::createWorld(uint32_t seed, bool amplified)
{
	auto start = std::chrono::steady_clock::now();
	if(m_world)
		{
			m_renderer.waitIdle();
			m_world->saveAll();
			m_renderer.clearWorldMeshes();
			m_lod.reset(); // threads du LOD arrêtés avant le générateur
			m_world.reset();
		}
	m_world = std::make_unique<World>(seed, amplified);
	// Monde par défaut dans "world" (compatibilité), les autres à part
	std::string dir = m_dataDir + "/world";
	if(seed != 42 || amplified)
		dir = m_dataDir + "/worlds/" + std::to_string(seed) + (amplified ? "-amplified" : "");
	if(m_script.empty()) // les scripts de capture ne touchent pas aux sauvegardes
		m_world->setWorldDir(dir);
	m_world->setRenderRadius(m_settings.renderDistance);
	m_lod = std::make_unique<LodTerrain>(m_world->getGenerator());
	m_leaves = LeafParticles{};
	m_spawned = false;
	findScenicSpawn();
	double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	LOG_INFO("World seed " << seed << (amplified ? " (amplified)" : "") << " ready in " << ms << " ms");
}

void Application::flyToView(const glm::vec3 &target, float distance, float height)
{
	const TerrainGenerator &gen = m_world->getGenerator();
	// Direction la plus dégagée : relief le plus bas entre la caméra et la cible
	float bestCost = 1e9f, bestA = 0.0f;
	for(int i = 0; i < 16; ++i)
		{
			float a = i * 3.14159265f / 8.0f;
			float cost = 0.0f;
			for(float t = 0.15f; t <= 1.0f; t += 0.15f)
				{
					glm::vec2 p = glm::vec2(target.x, target.z) + glm::vec2(std::cos(a), std::sin(a)) * distance * t;
					float sight = target.y + height * t;
					cost = std::max(cost, gen.getHeightAt(static_cast<int>(p.x), static_cast<int>(p.y)) - sight);
				}
			if(cost < bestCost)
				{
					bestCost = cost;
					bestA = a;
				}
		}
	glm::vec3 eye(target.x + std::cos(bestA) * distance, 0.0f, target.z + std::sin(bestA) * distance);
	float ground = static_cast<float>(gen.getHeightAt(static_cast<int>(eye.x), static_cast<int>(eye.z)));
	eye.y = std::max({target.y + height, ground + 3.0f, static_cast<float>(SEA_LEVEL) + 2.0f});
	eye.y = std::min(eye.y, 300.0f);

	glm::vec3 d = target - eye;
	float yaw = glm::degrees(std::atan2(d.z, d.x));
	float pitch = glm::degrees(std::atan2(d.y, glm::length(glm::vec2(d.x, d.z))));
	m_player.setFlying(true);
	m_player.setPosition(eye - glm::vec3(0.0f, 1.62f, 0.0f));
	m_player.setLook(yaw, std::clamp(pitch, -60.0f, 30.0f));
	m_spawned = true;
	m_menu.setActive(false);
}

void Application::teleportToStructure(StructureType type, float distanceScale)
{
	glm::vec3 p = m_player.getPosition();
	StructureSite site;
	if(!m_world->getGenerator().structures().findNearest(
		 type, glm::ivec2(static_cast<int>(p.x), static_cast<int>(p.z)), 10000, site))
		{
			notify(std::string("No ") + structureName(type) + " found nearby");
			return;
		}
	float r = static_cast<float>(site.radius);
	float lift = type == StructureType::Lighthouse ? 14.0f : (type == StructureType::DesertTemple ? 6.0f : 3.0f);
	flyToView(glm::vec3(site.center.x + 0.5f, site.center.y + lift, site.center.z + 0.5f),
			  (r * 0.85f + 16.0f) * distanceScale, (r * 0.35f + 7.0f) * distanceScale);
	int dist = static_cast<int>(glm::length(glm::vec2(site.center.x - p.x, site.center.z - p.z)));
	notify(std::string(structureName(type)) + " (" + std::to_string(dist) + " blocks away)");
}

void Application::teleportToBiome(BiomeType biome)
{
	const TerrainGenerator &gen = m_world->getGenerator();
	glm::vec3 p = m_player.getPosition();
	glm::ivec2 from(static_cast<int>(p.x), static_cast<int>(p.z));
	const int STEP = 48;
	bool found = false;
	glm::ivec2 hit(0);
	for(int r = 1; r <= 8000 / STEP && !found; ++r)
		for(int i = -r; i <= r && !found; ++i)
			for(int k = 0; k < 4 && !found; ++k)
				{
					glm::ivec2 o = k == 0 ? glm::ivec2(i, -r) : k == 1 ? glm::ivec2(i, r)
															  : k == 2 ? glm::ivec2(-r, i) : glm::ivec2(r, i);
					glm::ivec2 q = from + o * STEP;
					if(gen.getBiomeAt(q.x, q.y) == biome)
						{
							found = true;
							hit = q;
						}
				}
	if(!found)
		{
			notify("No " + getBiomeData(biome).name + " found nearby");
			return;
		}
	// Point remarquable alentour : le plus haut (pics) ou le plus central
	glm::ivec2 best = hit;
	int bestH = gen.getHeightAt(hit.x, hit.y);
	if(biome == BiomeType::Amplified || biome == BiomeType::SnowyPeaks)
		for(int dx = -256; dx <= 256; dx += 12)
			for(int dz = -256; dz <= 256; dz += 12)
				{
					glm::ivec2 q = hit + glm::ivec2(dx, dz);
					int h = gen.getHeightAt(q.x, q.y);
					if(h > bestH && gen.getBiomeAt(q.x, q.y) == biome)
						{
							bestH = h;
							best = q;
						}
				}
	bool peaks = biome == BiomeType::Amplified || biome == BiomeType::SnowyPeaks;
	glm::vec3 target(best.x + 0.5f, bestH - (peaks ? 25.0f : -2.0f), best.y + 0.5f);
	flyToView(target, peaks ? 150.0f : 45.0f, peaks ? 10.0f : 22.0f);
	int dist = static_cast<int>(glm::length(glm::vec2(best - from)));
	notify(getBiomeData(biome).name + " (" + std::to_string(dist) + " blocks away)");
}

void Application::handleWorldRequest(const WorldRequest &req)
{
	switch(req.kind)
		{
		case WorldRequest::LocateStructure: teleportToStructure(req.structure); break;
		case WorldRequest::LocateBiome: teleportToBiome(req.biome); break;
		case WorldRequest::NewWorld:
			createWorld(req.seed, req.amplified);
			notify("New world: seed " + std::to_string(req.seed)
				   + (req.amplified ? " (amplified)" : ""));
			break;
		default: break;
		}
}

// ── Préréglages de shaders ──────────────────────────────────────
// Golden Hour / Horreur : soleil devant la caméra (contre-jour, rayons dans
// la brume) ; Réaliste : soleil derrière, comme au point d'apparition.
void Application::onPresetChanged()
{
	if(m_settings.preset == ShaderPreset::Realistic)
		{
			m_settings.sunAzimuth = m_spawnSunAzimuth;
			return;
		}
	glm::vec3 f = m_camera.getFront();
	float viewAzimuth = glm::degrees(std::atan2(f.x, f.z));
	float offset = m_settings.preset == ShaderPreset::GoldenHour ? 12.0f : -8.0f;
	// Nuit : la lune (opposée au soleil) se lève devant, un peu de côté
	if(m_settings.preset == ShaderPreset::Night)
		offset = 180.0f + 62.0f;
	m_settings.sunAzimuth = std::fmod(viewAzimuth + offset + 720.0f, 360.0f);
}

// ── Données de l'interface (onglets Performance / Système) ──────
void Application::updateDebugInfo(float time)
{
	DebugInfo &d = m_debug;
	d.chunksLoaded = m_world->getChunkCount();
	d.chunksMeshed = m_world->getMeshedCount();
	d.pendingJobs = m_world->getPendingJobs();
	d.workerThreads = m_world->getWorkerCount();
	d.renderRadius = m_world->getRenderRadius();
	d.position = m_gameStarted ? m_player.getPosition() : m_camera.getPosition();
	d.worldSeed = m_world->getGenerator().getSeed();
	d.amplifiedWorld = m_world->getGenerator().isAmplifiedWorld();
	d.flying = m_player.isFlying();
	ColumnInfo col = m_world->getGenerator().getColumn(static_cast<int>(std::floor(d.position.x)),
													  static_cast<int>(std::floor(d.position.z)));
	d.biome = getBiomeData(col.biome).name;
	d.render = m_renderer.getStats();
	d.lodTiles = d.render.lodTiles;
	d.cpuThreads = std::thread::hardware_concurrency();
	glfwGetWindowSize(m_window, &d.windowW, &d.windowH);
	if(const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor()))
		{
			d.monitorW = mode->width;
			d.monitorH = mode->height;
			d.monitorHz = mode->refreshRate;
		}
	d.fullscreen = m_fullscreen;
	d.uptime = time;
	static int s_deviceRefresh = 0;
	if(s_deviceRefresh++ % 60 == 0) // change rarement (VSync, ombres)
		d.device = m_renderer.getDeviceInfo();
}

// ── Caméra ──────────────────────────────────────────────────────
void Application::updateCamera(float time)
{
	int fbW, fbH;
	glfwGetFramebufferSize(m_window, &fbW, &fbH);
	if(fbW > 0 && fbH > 0)
		m_camera.setAspectRatio(static_cast<float>(fbW) / static_cast<float>(fbH));
	// Sprint : champ de vision un peu plus large ; C : zoom (lunette)
	bool zoom = m_gameStarted && !m_menu.isActive() && m_cursorCaptured
				&& glfwGetKey(m_window, GLFW_KEY_C) == GLFW_PRESS;
	float targetScale = zoom ? 0.25f : (m_player.isSprinting() ? 1.12f : 1.0f);
	m_fovScale += (targetScale - m_fovScale) * (zoom ? 0.25f : 0.12f);
	m_camera.setFov(std::clamp(m_settings.fov * m_fovScale, 10.0f, 130.0f));

	if(!m_gameStarted)
		{
			// Menu : lent panoramique depuis le point d'apparition
			float yaw = glm::radians(m_spawnYaw + 18.0f * std::sin(time * 0.05f));
			float pitch = glm::radians(5.0f);
			m_camera.setPosition(m_spawnPos + glm::vec3(0.0f, 2.4f, 0.0f));
			m_camera.setDirection(glm::vec3(std::cos(yaw) * std::cos(pitch), std::sin(pitch),
											std::sin(yaw) * std::cos(pitch)));
			return;
		}
	m_camera.setPosition(m_player.getEyePosition());
	m_camera.setDirection(m_player.getFrontVector());
}

// ── Construction ────────────────────────────────────────────────
// Parcours des voxels traversés par le regard (Amanatides & Woo)
Application::RayHit Application::raycastBlock(float maxDist) const
{
	RayHit r;
	glm::vec3 o = m_player.getEyePosition(), d = m_player.getFrontVector();
	glm::ivec3 cell(std::floor(o.x), std::floor(o.y), std::floor(o.z));
	glm::ivec3 step(d.x > 0 ? 1 : -1, d.y > 0 ? 1 : -1, d.z > 0 ? 1 : -1);
	glm::vec3 tDelta, tMax;
	for(int i = 0; i < 3; ++i)
		{
			tDelta[i] = std::abs(d[i]) > 1e-6f ? std::abs(1.0f / d[i]) : 1e30f;
			float edge = d[i] > 0 ? (cell[i] + 1.0f - o[i]) : (o[i] - cell[i]);
			tMax[i] = std::abs(d[i]) > 1e-6f ? edge * tDelta[i] : 1e30f;
		}
	glm::ivec3 normal(0);
	float t = 0.0f;
	while(t <= maxDist)
		{
			BlockType b = m_world->getBlockAt(cell.x, cell.y, cell.z);
			if(b != BlockType::Air && b != BlockType::Water)
				{
					r.hit = true;
					r.block = cell;
					r.normal = normal;
					r.type = b;
					return r;
				}
			int axis = tMax.x < tMax.y ? (tMax.x < tMax.z ? 0 : 2) : (tMax.y < tMax.z ? 1 : 2);
			cell[axis] += step[axis];
			t = tMax[axis];
			tMax[axis] += tDelta[axis];
			normal = glm::ivec3(0);
			normal[axis] = -step[axis];
		}
	return r;
}

void Application::updateBuilding(float time)
{
	auto pressed = [&](int button) { return glfwGetMouseButton(m_window, button) == GLFW_PRESS; };
	bool lmb = pressed(GLFW_MOUSE_BUTTON_LEFT), rmb = pressed(GLFW_MOUSE_BUTTON_RIGHT),
		 mmb = pressed(GLFW_MOUSE_BUTTON_MIDDLE);

	// Choix de l'emplacement : touches 1..9, molette
	int slot = m_hotbarSlot;
	for(int k = 0; k < 9; ++k)
		if(glfwGetKey(m_window, GLFW_KEY_1 + k) == GLFW_PRESS)
			slot = k;
	if(std::abs(m_scrollAccum) >= 1.0)
		{
			int steps = static_cast<int>(m_scrollAccum);
			slot = ((slot - steps) % 9 + 9) % 9;
			m_scrollAccum -= steps;
		}
	if(slot != m_hotbarSlot)
		{
			m_hotbarSlot = slot;
			notify(BlockDatabase::Get(m_hotbar[slot]).name);
		}

	m_target = raycastBlock(7.0f);
	if(m_target.hit)
		{
			const glm::ivec3 &b = m_target.block;
			if(lmb && (!m_lmbWas || time >= m_nextBreak) && m_target.type != BlockType::Bedrock)
				{
					m_world->setBlockAt(b.x, b.y, b.z, BlockType::Air);
					m_nextBreak = time + 0.22f;
				}
			if(rmb && (!m_rmbWas || time >= m_nextPlace))
				{
					glm::ivec3 p = b + m_target.normal;
					BlockType cur = m_world->getBlockAt(p.x, p.y, p.z);
					bool replaceable = cur == BlockType::Air || cur == BlockType::Water
									   || BlockDatabase::Get(cur).renderType == RenderType::Cross;
					// Pas de bloc plein dans le joueur
					glm::vec3 pp = m_player.getPosition();
					bool inside = pp.x + 0.3f > p.x && pp.x - 0.3f < p.x + 1 && pp.z + 0.3f > p.z
								  && pp.z - 0.3f < p.z + 1 && pp.y + 1.8f > p.y && pp.y < p.y + 1;
					BlockType sel = m_hotbar[m_hotbarSlot];
					if(replaceable && !(inside && BlockDatabase::IsSolid(sel)))
						m_world->setBlockAt(p.x, p.y, p.z, sel);
					m_nextPlace = time + 0.22f;
				}
			if(mmb && !m_mmbWas && m_renderer.getBlockIcon(m_target.type))
				{
					m_hotbar[m_hotbarSlot] = m_target.type; // pipette
					notify(BlockDatabase::Get(m_target.type).name);
				}
		}
	m_lmbWas = lmb;
	m_rmbWas = rmb;
	m_mmbWas = mmb;
}

void Application::drawBlockOutline(const RayHit &hit)
{
	if(!hit.hit)
		return;
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	glm::mat4 vp = m_camera.getProjectionMatrix() * m_camera.getViewMatrix();
	glm::vec3 cam = m_camera.getPosition();
	glm::vec3 lo = glm::vec3(hit.block) - 0.003f - cam, hi = glm::vec3(hit.block) + 1.003f - cam;
	glm::vec4 c[8];
	for(int i = 0; i < 8; ++i)
		c[i] = vp * glm::vec4(i & 1 ? hi.x : lo.x, i & 2 ? hi.y : lo.y, i & 4 ? hi.z : lo.z, 1.0f);
	const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
							  {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
	ImDrawList *dl = ImGui::GetForegroundDrawList();
	auto screen = [&](glm::vec4 p) {
		return ImVec2((p.x / p.w * 0.5f + 0.5f) * display.x, (p.y / p.w * 0.5f + 0.5f) * display.y);
	};
	for(const auto &e : edges)
		{
			glm::vec4 a = c[e[0]], b = c[e[1]];
			const float nearW = 0.05f;
			if(a.w < nearW && b.w < nearW)
				continue;
			// Découpe contre le plan proche
			if(a.w < nearW)
				a = glm::mix(a, b, (nearW - a.w) / (b.w - a.w));
			if(b.w < nearW)
				b = glm::mix(b, a, (nearW - b.w) / (a.w - b.w));
			dl->AddLine(screen(a), screen(b), IM_COL32(20, 20, 20, 190), 2.0f);
		}
}

void Application::drawHotbar()
{
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImDrawList *dl = ImGui::GetForegroundDrawList();
	const float slot = 44.0f, gap = 4.0f;
	float total = 9 * slot + 8 * gap;
	ImVec2 o(display.x * 0.5f - total * 0.5f, display.y - slot - 14.0f);
	dl->AddRectFilled(ImVec2(o.x - 6, o.y - 6), ImVec2(o.x + total + 6, o.y + slot + 6),
					  IM_COL32(0, 0, 0, 90), 6.0f);
	for(int i = 0; i < 9; ++i)
		{
			ImVec2 a(o.x + i * (slot + gap), o.y), b(a.x + slot, a.y + slot);
			dl->AddRectFilled(a, b, IM_COL32(30, 30, 34, 150), 3.0f);
			if(ImTextureID icon = m_renderer.getBlockIcon(m_hotbar[i]))
				dl->AddImage(icon, ImVec2(a.x + 6, a.y + 6), ImVec2(b.x - 6, b.y - 6));
			char key[2] = {static_cast<char>('1' + i), 0};
			dl->AddText(ImVec2(a.x + 3, a.y + 1), IM_COL32(255, 255, 255, 140), key);
			if(i == m_hotbarSlot)
				dl->AddRect(ImVec2(a.x - 2, a.y - 2), ImVec2(b.x + 2, b.y + 2),
							IM_COL32(255, 255, 255, 230), 4.0f, 0, 2.5f);
		}
}

// ── Notifications ───────────────────────────────────────────────
void Application::notify(const std::string &text)
{
	m_toast = text;
	m_toastTime = m_time;
}

void Application::drawNotification()
{
	float age = m_time - m_toastTime;
	if(age > 2.2f || m_toast.empty() || m_menu.isActive() || m_hideHud)
		return;
	float alpha = std::clamp((2.2f - age) / 0.5f, 0.0f, 1.0f);
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImDrawList *dl = ImGui::GetForegroundDrawList();
	ImVec2 ts = ImGui::CalcTextSize(m_toast.c_str());
	ImVec2 pos(display.x * 0.5f - ts.x * 0.5f, display.y * 0.78f);
	dl->AddRectFilled(ImVec2(pos.x - 12, pos.y - 6), ImVec2(pos.x + ts.x + 12, pos.y + ts.y + 6),
					  IM_COL32(0, 0, 0, static_cast<int>(120 * alpha)), 6.0f);
	dl->AddText(pos, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), m_toast.c_str());
}

// ── Overlay de debug (F3) ───────────────────────────────────────
void Application::drawHud(float deltaTime)
{
	const ImVec2 display = ImGui::GetIO().DisplaySize;

	if(m_hideHud)
		return;

	// Bloc visé, barre d'inventaire
	if(!m_menu.isActive() && m_gameStarted)
		{
			if(m_cursorCaptured)
				drawBlockOutline(m_target);
			drawHotbar();
		}

	// Réticule
	if(!m_menu.isActive() && m_cursorCaptured)
		{
			ImDrawList *dl = ImGui::GetForegroundDrawList();
			ImVec2 c(display.x * 0.5f, display.y * 0.5f);
			dl->AddLine(ImVec2(c.x - 8, c.y), ImVec2(c.x + 8, c.y), IM_COL32(255, 255, 255, 170), 2.0f);
			dl->AddLine(ImVec2(c.x, c.y - 8), ImVec2(c.x, c.y + 8), IM_COL32(255, 255, 255, 170), 2.0f);
		}

	// Indicateur de vol
	if(!m_menu.isActive() && m_player.isFlying())
		{
			ImDrawList *dl = ImGui::GetForegroundDrawList();
			const char *label = "FLYING";
			ImVec2 ts = ImGui::CalcTextSize(label);
			ImVec2 p(display.x - ts.x - 18.0f, display.y - ts.y - 16.0f);
			dl->AddRectFilled(ImVec2(p.x - 8, p.y - 4), ImVec2(p.x + ts.x + 8, p.y + ts.y + 4),
							  IM_COL32(0, 0, 0, 90), 4.0f);
			dl->AddText(p, IM_COL32(235, 245, 255, 220), label);
		}

	static float s_fps = 60.0f;
	if(deltaTime > 0.0f)
		s_fps = s_fps * 0.95f + (1.0f / deltaTime) * 0.05f;

	if(!m_showDebug || m_menu.isActive())
		return;

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
							 | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
							 | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
	ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.45f);
	ImGui::Begin("Debug", nullptr, flags);

	glm::vec3 pos = m_player.getPosition();
	glm::vec3 vel = m_player.getVelocity();
	glm::vec3 front = m_player.getFrontVector();
	const Renderer::Stats &rs = m_renderer.getStats();

	ImGui::Text("Farlands  %d fps (%.2f ms)", static_cast<int>(s_fps), 1000.0f / s_fps);
	ImGui::Separator();
	ImGui::Text("XYZ: %.2f / %.2f / %.2f", pos.x, pos.y, pos.z);
	ImGui::Text("Chunk: %d, %d", m_player.getChunkX(), m_player.getChunkZ());
	const char *facing = std::abs(front.x) > std::abs(front.z)
						   ? (front.x > 0 ? "East (+X)" : "West (-X)")
						   : (front.z > 0 ? "South (+Z)" : "North (-Z)");
	ImGui::Text("Facing: %s", facing);
	ImGui::Text("Velocity: %.1f, %.1f, %.1f  %s%s%s", vel.x, vel.y, vel.z,
				m_player.isOnGround() ? "[ground]" : "", m_player.isInWater() ? "[water]" : "",
				m_player.isFlying() ? "[flying]" : "");
	ImGui::Separator();
	ImGui::Text("Chunks: %d loaded, %d meshed, %d on GPU (%zu MB)", m_world->getChunkCount(),
				m_world->getMeshedCount(), rs.chunks, rs.gpuMemoryMB);
	ImGui::Text("Drawn: %d  shadow casters: %d  jobs: %zu", rs.drawnChunks, rs.shadowChunks,
				m_world->getPendingJobs());
	if(rs.gpuMs > 0.0f)
		{
			std::ostringstream gpu;
			gpu << std::fixed << std::setprecision(2) << "GPU " << rs.gpuMs << " ms:";
			for(int i = 0; i < Renderer::PASS_COUNT; ++i)
				gpu << " " << Renderer::passName(i) << " " << rs.passMs[i];
			ImGui::TextUnformatted(gpu.str().c_str());
		}

	int wx = static_cast<int>(std::floor(pos.x));
	int wz = static_cast<int>(std::floor(pos.z));
	ColumnInfo col = m_world->getGenerator().getColumn(wx, wz);
	const BiomeData &bd = getBiomeData(col.biome);
	ImGui::Separator();
	ImGui::ColorButton("##biome", ImVec4(bd.color.r, bd.color.g, bd.color.b, 1.0f),
					   ImGuiColorEditFlags_NoTooltip, ImVec2(12, 12));
	ImGui::SameLine();
	ImGui::Text("Biome: %s  (temp %.0f%%, humidity %.0f%%)", bd.name.c_str(),
				col.temperature * 100.0f, col.humidity * 100.0f);
	ImGui::Text("Terrain height: %d  snow line: %d", col.height,
				m_world->getGenerator().getSnowLine(wx, wz));

	// Bloc visé
	glm::vec3 eye = m_player.getEyePosition();
	for(float t = 0; t < 6.0f; t += 0.05f)
		{
			glm::vec3 p = eye + front * t;
			glm::ivec3 b(std::floor(p.x), std::floor(p.y), std::floor(p.z));
			BlockType bt = m_world->getBlockAt(b.x, b.y, b.z);
			if(bt != BlockType::Air && bt != BlockType::Water)
				{
					ImGui::Text("Looking at: %s [%d, %d, %d]", BlockDatabase::Get(bt).name.c_str(),
								b.x, b.y, b.z);
					break;
				}
		}
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
					   "WASD move | Space jump/swim | Ctrl sprint | Space x2 / F fly (Shift down) | C zoom");
	ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
					   "LMB break | RMB place | MMB pick | 1-9 / wheel block | F1 hide HUD");
	ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
					   "Esc menu | Tab cursor | F2 screenshot | F3 debug | F11 fullscreen");
	ImGui::End();
}

// ── Boucle principale ───────────────────────────────────────────
void Application::mainLoop()
{
	auto lastTime = std::chrono::high_resolution_clock::now();
	auto startTime = lastTime;
	bool tabWasPressed = false, escWasPressed = false;
	bool f1WasPressed = false, f2WasPressed = false, f3WasPressed = false, f11WasPressed = false;

	while(!glfwWindowShouldClose(m_window))
		{
			glfwPollEvents();

			auto currentTime = std::chrono::high_resolution_clock::now();
			float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
			float time = std::chrono::duration<float>(currentTime - startTime).count();
			lastTime = currentTime;
			m_time = time;
			const float rawDeltaMs = deltaTime * 1000.0f; // statistiques
			if(deltaTime > 0.1f)
				deltaTime = 0.1f;

			// ── Clavier ─────────────────────────────────────────
			auto edge = [&](int key, bool &was) {
				bool now = glfwGetKey(m_window, key) == GLFW_PRESS;
				bool pressed = now && !was;
				was = now;
				return pressed;
			};
			if(edge(GLFW_KEY_ESCAPE, escWasPressed) && !m_menu.isActive())
				m_menu.setActive(true);
			if(edge(GLFW_KEY_F2, f2WasPressed))
				m_renderer.requestScreenshot(makeScreenshotPath());
			if(edge(GLFW_KEY_F3, f3WasPressed))
				m_showDebug = !m_showDebug;
			if(edge(GLFW_KEY_F11, f11WasPressed))
				toggleFullscreen();
			if(edge(GLFW_KEY_F1, f1WasPressed))
				m_hideHud = !m_hideHud;
			if(edge(GLFW_KEY_TAB, tabWasPressed) && !m_menu.isActive())
				setCursorCaptured(!m_cursorCaptured);

			// Curseur : capturé en entrant en jeu, libéré dans le menu
			// (uniquement sur les transitions, sinon Tab serait annulé)
			if(m_menu.isActive() != m_menuWasActive)
				{
					LOG_NOTE((m_menu.isActive() ? "Main Menu opened" : "Main Menu closed"));
					if(!m_menu.isActive())
						m_gameStarted = true;
					setCursorCaptured(!m_menu.isActive());
					m_menuWasActive = m_menu.isActive();
				}

			// ── Monde : streaming autour de la caméra ───────────
			auto tWorld = std::chrono::steady_clock::now();
			m_world->setRenderRadius(m_settings.renderDistance);
			glm::vec3 focus = m_gameStarted ? m_player.getPosition() : m_spawnPos;
			m_world->update(focus);
			auto tUpload = std::chrono::steady_clock::now();
			for(auto &ev : m_world->takeMeshEvents(48))
				{
					if(ev.mesh)
						m_renderer.setChunkMesh(*ev.mesh);
					else
						m_renderer.removeChunkMesh(ev.pos);
				}
			// Terrain lointain
			m_lod->update(m_gameStarted ? m_player.getPosition() : m_spawnPos,
						 m_settings.lodDistance, m_world->getRenderRadius());
			for(auto &ev : m_lod->takeEvents())
				{
					if(ev.mesh)
						m_renderer.setLodMesh(*ev.mesh);
					else
						m_renderer.removeLodMesh(ev.tile);
				}
			auto tEnd = std::chrono::steady_clock::now();
			auto msBetween = [](auto a, auto b) {
				return std::chrono::duration<float, std::milli>(b - a).count();
			};
			m_debug.worldUpdateMs = m_debug.worldUpdateMs * 0.9f + msBetween(tWorld, tUpload) * 0.1f;
			m_debug.uploadMs = m_debug.uploadMs * 0.9f + msBetween(tUpload, tEnd) * 0.1f;

			// Pose du joueur une fois son chunk chargé
			if(!m_spawned && m_world->isColumnLoaded(static_cast<int>(m_spawnPos.x),
													static_cast<int>(m_spawnPos.z)))
				{
					m_player.setPosition(m_spawnPos);
					m_player.spawnOnSurface(*m_world, static_cast<int>(m_spawnPos.y));
					m_spawned = true;
				}

			// ── Joueur (uniquement en jeu, terrain chargé) ──────
			glm::vec3 pp = m_player.getPosition();
			bool groundLoaded = m_world->isColumnLoaded(static_cast<int>(std::floor(pp.x)),
													   static_cast<int>(std::floor(pp.z)));
			m_player.setFlySpeed(m_settings.flySpeed);
			if(!m_menu.isActive() && m_spawned && groundLoaded)
				{
					m_player.handleInput(m_window, deltaTime, *m_world);
					m_player.update(deltaTime, *m_world);
				}
			if(!m_menu.isActive() && m_cursorCaptured && m_spawned && groundLoaded)
				updateBuilding(time);
			else
				m_target = RayHit{};
			if(m_player.consumeFlyToggled())
				notify(m_player.isFlying() ? "Flying (Space up, Shift down, Ctrl faster)"
										   : "Walking");
			updateScript(time);

			updateCamera(time);

			// ── Feuilles qui tombent (autour de la caméra) ──────
			m_leaves.update(deltaTime, time, *m_world, m_camera.getPosition(),
							m_settings.fallingLeaves);
			m_leaves.buildInstances(m_camera.getPosition(), m_particleInstances);
			m_renderer.setParticles(m_particleInstances);
			m_debug.particles = static_cast<int>(m_leaves.count());

			// ── Interface ───────────────────────────────────────
			auto tUi = std::chrono::steady_clock::now();
			m_debug.pushFrame(rawDeltaMs);
			updateDebugInfo(time);
			m_renderer.beginImGuiFrame();
			drawHud(deltaTime);
			drawNotification();
			if(m_settings.showFps && !m_menu.isActive())
				MainMenu::drawFpsCounter(m_debug);
			m_menu.draw(m_window, 0, (ImTextureID)m_renderer.getLogoTextureId(), m_settings,
						m_gameStarted, m_debug);
			if(m_menu.consumePresetChanged())
				onPresetChanged();
			handleWorldRequest(m_menu.consumeWorldRequest());
			m_debug.uiMs = m_debug.uiMs * 0.9f
						   + std::chrono::duration<float, std::milli>(
							   std::chrono::steady_clock::now() - tUi).count() * 0.1f;

			// ── Rendu ───────────────────────────────────────────
			glm::vec3 eye = m_camera.getPosition();
			FrameParams params;
			params.camera = &m_camera;
			params.time = time;
			// Rayon effectivement chargé (le monde plafonne la distance de rendu)
			float chunkDist = static_cast<float>(m_world->getRenderRadius() * CHUNK_SIZE_X);
			params.viewDistance = std::max(chunkDist, static_cast<float>(m_settings.lodDistance));
			params.lodInnerRadius = std::max(0.0f, chunkDist - 1.2f * CHUNK_SIZE_X);
			params.underwater = m_world->getBlockAt(static_cast<int>(std::floor(eye.x)),
												   static_cast<int>(std::floor(eye.y)),
												   static_cast<int>(std::floor(eye.z)))
								== BlockType::Water;
			m_renderer.drawFrame(params, m_settings);
		}

	m_renderer.waitIdle();
}

// Détruit de manière propre la fenêtre et le Renderer Vulkan
void Application::cleanup()
{
	m_world->saveAll();
	m_renderer.cleanup();
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

// Coordonne l'initialisation complète
void Application::run()
{
	initWindow();

	// Charger la base de données de blocs depuis le JSON d'abord !
	LOG_INFO("Loading block database...");
	BlockDatabase::LoadFromFile(std::string(ASSETS_DIR) + "/data/blocks.json");

	std::string homeDir;
#ifdef _WIN32
	homeDir = std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : ".";
#else
	homeDir = std::getenv("HOME") ? std::getenv("HOME") : ".";
#endif
	m_dataDir = homeDir + "/.farlands";
	uint32_t seed = 42;
	if(const char *env = std::getenv("FARLANDS_SEED"))
		seed = static_cast<uint32_t>(std::strtoul(env, nullptr, 10));
	const char *amp = std::getenv("FARLANDS_AMPLIFIED");
	if(const char *script = std::getenv("FARLANDS_SCRIPT"))
		loadScript(script);
	createWorld(seed, amp && amp[0] == '1');

	LOG_INFO("Initializing Vulkan renderer...");
	m_renderer.init(m_window, m_settings);
	LOG_SUCCESS("Vulkan renderer initialized. World streaming in the background.");

	mainLoop();
	cleanup();
}
