// -----------------------------------------------------------------------------
// Fichier : app.cpp
// Rôle : Implémentation du cœur de l'application (Application).
// Gère l'initialisation de GLFW, la boucle principale avec le calcul MVP,
// le contrôleur joueur FPS, le monde infini et ImGui.
// -----------------------------------------------------------------------------
#include "app.h"
#include "world/biome.h"
#include "world/block.h"
#include "core/logger.h"

#include <chrono>
#include <cstdlib>
#include <glm/gtc/matrix_transform.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>

// Callback pour ajuster le viewport Vulkan lors d'un resize de fenêtre GLFW
void Application::framebufferResizeCallback(GLFWwindow *window, int, int)
{
	auto app
	  = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	app->m_renderer.framebufferResized = true;
}

// Callback souris — route les mouvements vers le Player FPS
void Application::cursorPosCallback(GLFWwindow *window, double xpos,
									double ypos)
{
	auto app
	  = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));

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
	if(!m_window) {
		LOG_ERROR("Failed to create GLFW window!");
		throw std::runtime_error("Failed to create GLFW window");
	}
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
	glfwSetCursorPosCallback(m_window, cursorPosCallback);
	LOG_SUCCESS("Window initialized successfully.");
}

// Boucle principale
void Application::mainLoop()
{
	auto lastTime = std::chrono::high_resolution_clock::now();
	bool tabWasPressed = false;
	bool escWasPressed = false;
	bool f2WasPressed = false;

	while(!glfwWindowShouldClose(m_window))
		{
			glfwPollEvents();

			// ── DeltaTime ───────────────────────────────────────
			auto currentTime = std::chrono::high_resolution_clock::now();
			float deltaTime
			  = std::chrono::duration<float>(currentTime - lastTime).count();
			lastTime = currentTime;

			if(deltaTime > 0.1f)
				deltaTime = 0.1f;

			// ── ESC : retour au menu principal (pause) ─────────
			bool escPressed
			  = glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
			if(escPressed && !escWasPressed && !m_menu.isActive())
				m_menu.setActive(true);
			escWasPressed = escPressed;

			// ── F2 : capture d'écran ────────────────────────────
			bool f2Pressed = glfwGetKey(m_window, GLFW_KEY_F2) == GLFW_PRESS;
			if(f2Pressed && !f2WasPressed)
				m_renderer.requestScreenshot(makeScreenshotPath());
			f2WasPressed = f2Pressed;

			// ── Toggle curseur (Tab, en jeu uniquement) ────────
			bool tabPressed
			  = glfwGetKey(m_window, GLFW_KEY_TAB) == GLFW_PRESS;
			if(tabPressed && !tabWasPressed && !m_menu.isActive())
				setCursorCaptured(!m_cursorCaptured);
			tabWasPressed = tabPressed;

			// ── Curseur : capturé en entrant en jeu, libéré dans
			// le menu (uniquement sur les transitions, sinon le
			// toggle Tab serait annulé à la frame suivante) ─────
			if(m_menu.isActive() != m_menuWasActive)
				{
					LOG_NOTE((m_menu.isActive() ? "Main Menu opened"
												: "Main Menu closed"));
					setCursorCaptured(!m_menu.isActive());
					m_menuWasActive = m_menu.isActive();
				}

			// ── Player update (seulement si pas dans le menu) ───
			if(!m_menu.isActive())
				{
					m_player.handleInput(m_window, deltaTime, m_world);
					m_player.update(deltaTime, m_world);
				}

			// ── Monde infini ─────────────────────────────────────
			int pcx = m_player.getChunkX();
			int pcz = m_player.getChunkZ();

			if(pcx != m_lastPlayerChunkX || pcz != m_lastPlayerChunkZ)
				{
					m_lastPlayerChunkX = pcx;
					m_lastPlayerChunkZ = pcz;

					bool changed = m_world.updateAroundPlayer(
					  pcx, pcz, RENDER_RADIUS);
					if(changed)
						{
							std::vector<Vertex> vertices;
							std::vector<uint32_t> indices;
							m_world.buildWorldMesh(vertices, indices);
							m_renderer.updateBuffers(vertices, indices);
						}
				}

			// ── Caméra ──────────────────────────────────────────
			int fbW, fbH;
			glfwGetFramebufferSize(m_window, &fbW, &fbH);
			if(fbW > 0 && fbH > 0)
				{
					m_camera.setAspectRatio(static_cast<float>(fbW)
											/ static_cast<float>(fbH));
				}

			glm::vec3 eyePos = m_player.getEyePosition();
			glm::vec3 front = m_player.getFrontVector();

			m_camera.setPosition(eyePos);
			m_camera.setTarget(eyePos + front);

			// ── ImGui ───────────────────────────────────────────
			m_renderer.beginImGuiFrame();

			ImGuiWindowFlags f3_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
			ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.5f); // Transparent background

			// Coordonnées ImGui = taille fenêtre (≠ framebuffer en Retina)
			const ImVec2 display = ImGui::GetIO().DisplaySize;

			ImGui::Begin("Player Info", nullptr, f3_flags);
			glm::vec3 pos = m_player.getPosition();
			glm::vec3 vel = m_player.getVelocity();
			
			// F3 style data
			static float s_fps = 60.0f;
			if(deltaTime > 0.0f)
				s_fps = s_fps * 0.95f + (1.0f / deltaTime) * 0.05f;
			
			ImGui::Text("Minecraft 2.0 (Farlands)");
			ImGui::Text("%d fps", static_cast<int>(s_fps));
			ImGui::Separator();
			
			ImGui::Text("XYZ: %.3f / %.5f / %.3f", pos.x, pos.y, pos.z);
			ImGui::Text("Chunk: %d 0 %d relative", m_player.getChunkX(), m_player.getChunkZ());
			
			// Facing string
			std::string facingStr;
			if(std::abs(front.x) > std::abs(front.z)) {
                facingStr = (front.x > 0) ? "East (Towards positive X)" : "West (Towards negative X)";
			} else {
                facingStr = (front.z > 0) ? "South (Towards positive Z)" : "North (Towards negative Z)";
			}
			ImGui::Text("Facing: %s", facingStr.c_str());
			
			ImGui::Text("Velocity: %.1f, %.1f, %.1f", vel.x, vel.y, vel.z);
			ImGui::Text("On Ground: %s", m_player.isOnGround() ? "true" : "false");
			ImGui::Text("C: %d", m_world.getChunkCount());
			ImGui::Separator();
			
			// Looking At logic (DDA Step Raycast)
			std::string raycastStr = "Air";
			glm::ivec3 targetPos(0);
			glm::vec3 rayPos = eyePos;
			for(float t = 0; t < 5.0f; t += 0.05f) {
				rayPos = eyePos + front * t;
				int bx = static_cast<int>(std::floor(rayPos.x));
				int by = static_cast<int>(std::floor(rayPos.y));
				int bz = static_cast<int>(std::floor(rayPos.z));
				BlockType bt = m_world.getBlockAt(bx, by, bz);
				if(BlockDatabase::IsSolid(bt)) {
					raycastStr = BlockDatabase::Get(bt).name;
					targetPos = glm::ivec3(bx, by, bz);
					break;
				}
			}
			if(raycastStr != "Air") {
				ImGui::Text("Targeted Block: %s [%d, %d, %d]", raycastStr.c_str(), targetPos.x, targetPos.y, targetPos.z);
			} else {
				ImGui::Text("Targeted Block: None");
			}
			ImGui::Separator();
			
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Tab: Toggle cursor | Esc: Menu");
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "WASD: Move | Space: Jump/Swim");
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "F2: Screenshot");
			ImGui::End();

			// Crosshair
			if (!m_menu.isActive() && m_cursorCaptured) {
				ImDrawList* drawList = ImGui::GetBackgroundDrawList();
				ImVec2 center(display.x / 2.0f, display.y / 2.0f);
				drawList->AddLine(ImVec2(center.x - 10, center.y), ImVec2(center.x + 10, center.y), IM_COL32(200, 200, 200, 150), 2.0f);
				drawList->AddLine(ImVec2(center.x, center.y - 10), ImVec2(center.x, center.y + 10), IM_COL32(200, 200, 200, 150), 2.0f);
			}

			// ── Biome Info ──────────────────────────────────────
			{
				const auto &bm = m_world.getBiomeManager();
				float wx = pos.x;
				float wz = pos.z;
				BiomeType biome = bm.getBiomeAt(wx, wz);
				const BiomeData &bd = BiomeManager::getData(biome);
				float temp = bm.getTemperature(wx, wz);
				float humidity = bm.getHumidity(wx, wz);

				// Biome Info en haut à droite
				ImGui::SetNextWindowPos(ImVec2(display.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
				ImGui::SetNextWindowBgAlpha(0.5f);

				ImGui::Begin("Biome", nullptr, f3_flags);

				// Indicateur de couleur du biome
				ImVec4 biomeCol(bd.color.r, bd.color.g, bd.color.b, 1.0f);
				ImGui::ColorButton("##biome_color", biomeCol,
								   ImGuiColorEditFlags_NoTooltip, ImVec2(15, 15));
				ImGui::SameLine();
				ImGui::Text("Biome: %s", bd.name.c_str());

				ImGui::Text("Temperature");
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.9f, 0.3f, 0.1f, 1.0f));
				ImGui::ProgressBar(temp, ImVec2(150, 14), (std::to_string(static_cast<int>(temp * 100)) + "%").c_str());
				ImGui::PopStyleColor();

				ImGui::Text("Humidity");
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
				ImGui::ProgressBar(humidity, ImVec2(150, 14), (std::to_string(static_cast<int>(humidity * 100)) + "%").c_str());
				ImGui::PopStyleColor();

				ImGui::Text("Surface: %s", BlockDatabase::Get(bd.surfaceBlock).name.c_str());
				ImGui::Text("Height: %d", bm.getHeightAt(wx, wz));

				ImGui::Separator();
				
				// Time Display
				std::time_t t = std::time(nullptr);
				std::tm* now = std::localtime(&t);
				std::ostringstream timeOss;
				timeOss << std::put_time(now, "%H:%M:%S");
				ImGui::Text("Local Time: %s", timeOss.str().c_str());

				ImGui::End();
			}

			// ── MVP ─────────────────────────────────────────────
			UniformBufferObject ubo{};
			ubo.model = glm::mat4(1.0f);
			ubo.view = m_camera.getViewMatrix();
			ubo.proj = m_camera.getProjectionMatrix();
			ubo.viewPos = eyePos; // Pour le brouillard dans le fragment shader
			ubo.time = static_cast<float>(glfwGetTime());
			ubo.invView = glm::inverse(ubo.view);
			ubo.invProj = glm::inverse(ubo.proj);
			// Le brouillard masque la limite des chunks chargés
			float renderDist = static_cast<float>(RENDER_RADIUS * CHUNK_SIZE_X);
			bool eyeInWater
			  = m_world.getBlockAt(static_cast<int>(std::floor(eyePos.x)),
								   static_cast<int>(std::floor(eyePos.y)),
								   static_cast<int>(std::floor(eyePos.z)))
				== BlockType::Water;
			if(eyeInWater)
				{
					ubo.fogRange = glm::vec2(1.0f, 24.0f);
					ubo.fogColor = glm::vec4(0.03f, 0.12f, 0.40f, 1.0f);
				}
			else
				{
					ubo.fogRange = glm::vec2(renderDist * 0.6f, renderDist);
					ubo.fogColor = glm::vec4(0.40f, 0.65f, 1.0f, 0.0f);
				}

			m_menu.draw(m_window, (ImTextureID)m_renderer.getPanoramaTextureId(), (ImTextureID)m_renderer.getLogoTextureId());

			m_renderer.drawFrame(ubo, m_menu.isActive());
		}

	m_renderer.waitIdle();
}

// Détruit de manière propre la fenêtre et le Renderer Vulkan
void Application::cleanup()
{
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
	LOG_SUCCESS("Block database loaded.");

	LOG_INFO("Initializing Vulkan renderer...");
	m_renderer.init(m_window);
	LOG_SUCCESS("Vulkan renderer initialized.");

	// Configurer le dossier de sauvegarde du monde
	std::string homeDir;
#ifdef _WIN32
	homeDir = std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : ".";
#else
	homeDir = std::getenv("HOME") ? std::getenv("HOME") : ".";
#endif
	m_dataDir = homeDir + "/.farlands";
	m_world.setWorldDir(m_dataDir + "/world");

	// Chargement initial autour du spawn
	m_lastPlayerChunkX = m_player.getChunkX();
	m_lastPlayerChunkZ = m_player.getChunkZ();
	LOG_INFO("Initial world setup around player...");
	m_world.updateAroundPlayer(m_lastPlayerChunkX, m_lastPlayerChunkZ,
							   RENDER_RADIUS);
	m_player.spawnOnSurface(m_world);

	// Construire le mesh initial
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	m_world.buildWorldMesh(vertices, indices);
	m_renderer.updateBuffers(vertices, indices);

	LOG_SUCCESS("World ready! Save dir: " << m_dataDir << "/world");

	mainLoop();
	cleanup();
}
