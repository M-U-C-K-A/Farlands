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

// Callback pour ajuster le viewport Vulkan lors d'un resize de fenêtre GLFW
void Application::framebufferResizeCallback(GLFWwindow *window, int, int)
{
	auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	app->m_renderer.framebufferResized = true;
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
	LOG_SUCCESS("Window initialized successfully.");
}

// ── Point d'apparition panoramique ──────────────────────────────
// Parcourt une grille autour de l'origine et note chaque position (et
// direction) : un lac juste devant, des sommets au loin, un sol herbeux.
void Application::findScenicSpawn()
{
	auto start = std::chrono::steady_clock::now();
	const TerrainGenerator &gen = m_world.getGenerator();
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

	double ms = std::chrono::duration<double, std::milli>(
				  std::chrono::steady_clock::now() - start)
				  .count();
	LOG_INFO("Scenic spawn at (" << bestPos.x << ", " << m_spawnPos.y << ", " << bestPos.y
								 << "), yaw " << m_spawnYaw << ", score " << bestScore
								 << " (" << ms << " ms)");
}

// ── Caméra ──────────────────────────────────────────────────────
void Application::updateCamera(float time)
{
	int fbW, fbH;
	glfwGetFramebufferSize(m_window, &fbW, &fbH);
	if(fbW > 0 && fbH > 0)
		m_camera.setAspectRatio(static_cast<float>(fbW) / static_cast<float>(fbH));
	m_camera.setFov(m_settings.fov);

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

// ── Overlay de debug (F3) ───────────────────────────────────────
void Application::drawHud(float deltaTime)
{
	const ImVec2 display = ImGui::GetIO().DisplaySize;

	// Réticule
	if(!m_menu.isActive() && m_cursorCaptured)
		{
			ImDrawList *dl = ImGui::GetForegroundDrawList();
			ImVec2 c(display.x * 0.5f, display.y * 0.5f);
			dl->AddLine(ImVec2(c.x - 8, c.y), ImVec2(c.x + 8, c.y), IM_COL32(255, 255, 255, 170), 2.0f);
			dl->AddLine(ImVec2(c.x, c.y - 8), ImVec2(c.x, c.y + 8), IM_COL32(255, 255, 255, 170), 2.0f);
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
	ImGui::Text("Velocity: %.1f, %.1f, %.1f  %s%s", vel.x, vel.y, vel.z,
				m_player.isOnGround() ? "[ground]" : "", m_player.isInWater() ? "[water]" : "");
	ImGui::Separator();
	ImGui::Text("Chunks: %d loaded, %d meshed, %d on GPU (%zu MB)", m_world.getChunkCount(),
				m_world.getMeshedCount(), rs.chunks, rs.gpuMemoryMB);
	ImGui::Text("Drawn: %d  shadow casters: %d  jobs: %zu", rs.drawnChunks, rs.shadowChunks,
				m_world.getPendingJobs());
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
	ColumnInfo col = m_world.getGenerator().getColumn(wx, wz);
	const BiomeData &bd = getBiomeData(col.biome);
	ImGui::Separator();
	ImGui::ColorButton("##biome", ImVec4(bd.color.r, bd.color.g, bd.color.b, 1.0f),
					   ImGuiColorEditFlags_NoTooltip, ImVec2(12, 12));
	ImGui::SameLine();
	ImGui::Text("Biome: %s  (temp %.0f%%, humidity %.0f%%)", bd.name.c_str(),
				col.temperature * 100.0f, col.humidity * 100.0f);
	ImGui::Text("Terrain height: %d  snow line: %d", col.height,
				m_world.getGenerator().getSnowLine(wx, wz));

	// Bloc visé
	glm::vec3 eye = m_player.getEyePosition();
	for(float t = 0; t < 6.0f; t += 0.05f)
		{
			glm::vec3 p = eye + front * t;
			glm::ivec3 b(std::floor(p.x), std::floor(p.y), std::floor(p.z));
			BlockType bt = m_world.getBlockAt(b.x, b.y, b.z);
			if(bt != BlockType::Air && bt != BlockType::Water)
				{
					ImGui::Text("Looking at: %s [%d, %d, %d]", BlockDatabase::Get(bt).name.c_str(),
								b.x, b.y, b.z);
					break;
				}
		}
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
					   "WASD move | Space jump/swim | Esc menu | Tab cursor | F2 screenshot | F3 debug");
	ImGui::End();
}

// ── Boucle principale ───────────────────────────────────────────
void Application::mainLoop()
{
	auto lastTime = std::chrono::high_resolution_clock::now();
	auto startTime = lastTime;
	bool tabWasPressed = false, escWasPressed = false;
	bool f2WasPressed = false, f3WasPressed = false;

	while(!glfwWindowShouldClose(m_window))
		{
			glfwPollEvents();

			auto currentTime = std::chrono::high_resolution_clock::now();
			float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
			float time = std::chrono::duration<float>(currentTime - startTime).count();
			lastTime = currentTime;
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
			m_world.setRenderRadius(m_settings.renderDistance);
			glm::vec3 focus = m_gameStarted ? m_player.getPosition() : m_spawnPos;
			m_world.update(focus);
			for(auto &ev : m_world.takeMeshEvents(48))
				{
					if(ev.mesh)
						m_renderer.setChunkMesh(*ev.mesh);
					else
						m_renderer.removeChunkMesh(ev.pos);
				}

			// Pose du joueur une fois son chunk chargé
			if(!m_spawned && m_world.isColumnLoaded(static_cast<int>(m_spawnPos.x),
													static_cast<int>(m_spawnPos.z)))
				{
					m_player.setPosition(m_spawnPos);
					m_player.spawnOnSurface(m_world, static_cast<int>(m_spawnPos.y));
					m_spawned = true;
				}

			// ── Joueur (uniquement en jeu, terrain chargé) ──────
			glm::vec3 pp = m_player.getPosition();
			bool groundLoaded = m_world.isColumnLoaded(static_cast<int>(std::floor(pp.x)),
													   static_cast<int>(std::floor(pp.z)));
			if(!m_menu.isActive() && m_spawned && groundLoaded)
				{
					m_player.handleInput(m_window, deltaTime, m_world);
					m_player.update(deltaTime, m_world);
				}

			updateCamera(time);

			// ── Interface ───────────────────────────────────────
			m_renderer.beginImGuiFrame();
			drawHud(deltaTime);
			m_menu.draw(m_window, 0, (ImTextureID)m_renderer.getLogoTextureId(), m_settings,
						m_gameStarted);

			// ── Rendu ───────────────────────────────────────────
			glm::vec3 eye = m_camera.getPosition();
			FrameParams params;
			params.camera = &m_camera;
			params.time = time;
			params.underwater = m_world.getBlockAt(static_cast<int>(std::floor(eye.x)),
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
	m_world.saveAll();
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
	m_world.setWorldDir(m_dataDir + "/world");
	m_world.setRenderRadius(m_settings.renderDistance);

	findScenicSpawn();

	LOG_INFO("Initializing Vulkan renderer...");
	m_renderer.init(m_window, m_settings);
	LOG_SUCCESS("Vulkan renderer initialized. World streaming in the background.");

	mainLoop();
	cleanup();
}
