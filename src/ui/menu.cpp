#include "menu.h"
#include "imgui.h"
#include <cmath>

bool MainMenu::draw(GLFWwindow *window, ImTextureID bg_texture, ImTextureID logo_texture)
{
	if(!m_active)
		return false;

	int w, h;
	glfwGetFramebufferSize(window, &w, &h);

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(
	  ImVec2(static_cast<float>(w), static_cast<float>(h)));
	ImGui::Begin("Main Menu", nullptr,
				 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);

	// Render background image filling the entire window with an animated scroll effect
	if (bg_texture) {
		float scroll = glfwGetTime() * 0.02f;
		ImGui::GetWindowDrawList()->AddImage(
			bg_texture,
			ImVec2(0, 0),
			ImVec2(static_cast<float>(w), static_cast<float>(h)),
			ImVec2(scroll, 0.0f),
			ImVec2(scroll + 1.0f, 1.0f)
		);
	}

	// Render Logo
	if(logo_texture) {
		float logoWidth = 600.0f;
		float logoHeight = 150.0f;
		ImGui::SetCursorPos(ImVec2(static_cast<float>(w) / 2.0f - logoWidth / 2.0f, static_cast<float>(h) / 2.0f - 200.0f));
		ImGui::Image(logo_texture, ImVec2(logoWidth, logoHeight));
	} else {
		// Fallback
		ImGui::SetCursorPos(ImVec2(static_cast<float>(w) / 2.0f - 100.0f, static_cast<float>(h) / 2.0f - 150.0f));
		ImGui::Text("Minecraft 2.0");
	}

	// Splash Text Bouncing
	float time = glfwGetTime();
	float scale = 1.0f + 0.1f * sin(time * 6.0f);
	ImGui::SetWindowFontScale(1.5f * scale);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
	
	// Position text slightly offset, overlapping the bottom right of the logo
	ImGui::SetCursorPos(ImVec2(static_cast<float>(w) / 2.0f + 100.0f, static_cast<float>(h) / 2.0f - 90.0f));
	// Rotate text slightly by drawing it creatively or just offset
	ImGui::TextUnformatted("Now with Vulkan!");
	ImGui::PopStyleColor();
	ImGui::SetWindowFontScale(1.0f); // Reset

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

	ImGui::SetCursorPos(
	  ImVec2(static_cast<float>(w) / 2 - 200, static_cast<float>(h) / 2 - 20));
	if(ImGui::Button("Singleplayer", ImVec2(400, 40)))
		{
			m_active = false;
		}

	ImGui::SetCursorPos(
	  ImVec2(static_cast<float>(w) / 2 - 200, static_cast<float>(h) / 2 + 30));
	if(ImGui::Button("Options", ImVec2(190, 40))) {}

	ImGui::SetCursorPos(
	  ImVec2(static_cast<float>(w) / 2 + 10, static_cast<float>(h) / 2 + 30));
	if(ImGui::Button("Quit", ImVec2(190, 40)))
		{
			glfwSetWindowShouldClose(window, true);
		}

	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar();

	ImGui::End();
	return !m_active; // true = player started game
}
