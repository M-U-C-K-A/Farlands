// -----------------------------------------------------------------------------
// Fichier : app.cpp
// Rôle : Implémentation du cœur de l'application (Application).
// Gère l'initialisation de GLFW, la boucle principale avec le calcul MVP et
// ImGui.
// -----------------------------------------------------------------------------
#include "app.h"
#include "world/block.h"

#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

// Callback pour ajuster le viewport Vulkan lors d'un resize de fenêtre GLFW
void Application::framebufferResizeCallback(GLFWwindow *window, int, int)
{
	auto app
	  = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	app->m_renderer.framebufferResized = true;
}

// Initialise le contexte de la fenêtre GLFW et définit ses callbacks
void Application::initWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(WIDTH, HEIGHT, "Farlands", nullptr, nullptr);
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
}

// Boucle principale de l'application: gère le temps, met à jour la logique,
// fait tourner la caméra, et orchestre le rendu Vulkan/ImGui pour chaque frame.
void Application::mainLoop()
{
	auto startTime = std::chrono::high_resolution_clock::now();

	while(!glfwWindowShouldClose(m_window))
		{
			glfwPollEvents();

			// ESC to close
			if(glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
				glfwSetWindowShouldClose(m_window, true);

			// Update MVP
			auto currentTime = std::chrono::high_resolution_clock::now();
			float time
			  = std::chrono::duration<float>(currentTime - startTime).count();

			int w, h;
			glfwGetFramebufferSize(m_window, &w, &h);
			if(w > 0 && h > 0)
				{
					m_camera.setAspectRatio(static_cast<float>(w)
											/ static_cast<float>(h));
				}

			UniformBufferObject ubo{};
			ubo.model = glm::mat4(
			  1.0f); // Pas de rotation, le monde est en world-space
			ubo.view = m_camera.getViewMatrix();
			ubo.proj = m_camera.getProjectionMatrix();

			// ImGui frame: begin → draw UI → drawFrame finishes
			m_renderer.beginImGuiFrame();
			m_menu.draw(m_window);

			m_renderer.drawFrame(ubo, m_menu.isActive());
		}

	m_renderer.waitIdle();
}

// Détruit de manière propre la fenêtre ainsi que le Renderer Vulkan
void Application::cleanup()
{
	m_renderer.cleanup();
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

// Coordonne l'initialisation complète de l'application, du parse JSON des
// blocs, la génération du monde et le démarrage du thread principal
void Application::run()
{
	initWindow();
	m_renderer.init(m_window);

	// Charger la base de données de blocs depuis le JSON
	BlockDatabase::LoadFromFile(std::string(ASSETS_DIR) + "/data/blocks.json");

	// Générer le monde : grille de 3x3 chunks (rayon 1)
	m_world.generate(2); // rayon 2 = grille 5x5 = 25 chunks

	// Construire le mesh monde complet
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	m_world.buildWorldMesh(vertices, indices);

	m_renderer.updateBuffers(vertices, indices);

	std::cout << "[App] World ready!" << std::endl;

	mainLoop();
	cleanup();
}
