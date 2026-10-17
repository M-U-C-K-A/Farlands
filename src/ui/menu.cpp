#include "menu.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace
	{
		bool bigButton(const char *label, ImVec2 size)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.10f, 0.14f, 0.55f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.32f, 0.42f, 0.75f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.45f, 0.58f, 0.9f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.25f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
			bool clicked = ImGui::Button(label, size);
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor(4);
			return clicked;
		}
	}

void MainMenu::draw(GLFWwindow *window, ImTextureID background, ImTextureID logo,
					RenderSettings &settings, bool gameStarted)
{
	if(m_showOptions)
		drawOptions(settings);
	if(!m_active)
		return;

	// Coordonnées ImGui (taille fenêtre, pas framebuffer : Retina)
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float w = display.x;
	const float h = display.y;

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(display);
	ImGui::Begin("Main Menu", nullptr,
				 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
				   | ImGuiWindowFlags_NoBringToFrontOnFocus);

	ImDrawList *dl = ImGui::GetWindowDrawList();
	if(background)
		{
			float scroll = static_cast<float>(glfwGetTime()) * 0.02f;
			dl->AddImage(background, ImVec2(0, 0), display, ImVec2(scroll, 0.0f),
						 ImVec2(scroll + 1.0f, 1.0f));
		}
	else
		{
			// Le monde est visible : léger voile en bas pour la lisibilité
			dl->AddRectFilledMultiColor(ImVec2(0, h * 0.45f), display,
										IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
										IM_COL32(0, 0, 0, 140), IM_COL32(0, 0, 0, 140));
		}

	// Logo
	float logoW = std::min(600.0f, w * 0.7f);
	float logoH = logoW * 125.0f / 409.0f;
	float logoY = h * 0.5f - 230.0f;
	if(logo)
		{
			ImGui::SetCursorPos(ImVec2(w * 0.5f - logoW * 0.5f, logoY));
			ImGui::Image(logo, ImVec2(logoW, logoH));
		}

	// Splash qui pulse
	float time = static_cast<float>(glfwGetTime());
	float scale = 1.0f + 0.08f * std::sin(time * 5.0f);
	ImGui::SetWindowFontScale(1.4f * scale);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.92f, 0.3f, 1.0f));
	ImGui::SetCursorPos(ImVec2(w * 0.5f + logoW * 0.22f, logoY + logoH - 10.0f));
	ImGui::TextUnformatted("Now with real shaders!");
	ImGui::PopStyleColor();
	ImGui::SetWindowFontScale(1.0f);

	const float bw = 400.0f, bh = 42.0f;
	float y = h * 0.5f + 10.0f;
	ImGui::SetCursorPos(ImVec2(w * 0.5f - bw * 0.5f, y));
	if(bigButton(gameStarted ? "Back to Game" : "Singleplayer", ImVec2(bw, bh)))
		{
			m_active = false;
			m_showOptions = false;
		}

	ImGui::SetCursorPos(ImVec2(w * 0.5f - bw * 0.5f, y + 52.0f));
	if(bigButton("Options...", ImVec2(bw * 0.5f - 5.0f, bh)))
		m_showOptions = !m_showOptions;

	ImGui::SetCursorPos(ImVec2(w * 0.5f + 5.0f, y + 52.0f));
	if(bigButton("Quit Game", ImVec2(bw * 0.5f - 5.0f, bh)))
		glfwSetWindowShouldClose(window, true);

	ImGui::SetCursorPos(ImVec2(10.0f, h - 26.0f));
	ImGui::TextColored(ImVec4(1, 1, 1, 0.6f), "Farlands - C++20 / Vulkan");
	ImGui::End();
}

void MainMenu::drawOptions(RenderSettings &s)
{
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(display.x - 20.0f, 20.0f), ImGuiCond_Appearing,
							ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Appearing);
	ImGui::SetNextWindowBgAlpha(0.85f);
	if(!ImGui::Begin("Options", &m_showOptions, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::End();
			return;
		}

	ImGui::PushItemWidth(200.0f);
	ImGui::SeparatorText("World");
	ImGui::SliderInt("Render distance", &s.renderDistance, 4, 128, "%d chunks");
	ImGui::SliderFloat("Field of view", &s.fov, 50.0f, 110.0f, "%.0f deg");

	ImGui::SeparatorText("Sun & sky");
	ImGui::SliderFloat("Sun elevation", &s.sunElevation, 1.0f, 89.0f, "%.0f deg");
	ImGui::SliderFloat("Sun azimuth", &s.sunAzimuth, 0.0f, 360.0f, "%.0f deg");
	ImGui::Checkbox("Volumetric clouds", &s.clouds);
	ImGui::SliderFloat("Cloud coverage", &s.cloudCoverage, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Haze", &s.fogDensity, 0.0f, 4.0f, "%.2f");

	ImGui::SeparatorText("Lighting");
	ImGui::Checkbox("Shadows", &s.shadows);
	const char *resNames[] = {"1024", "2048", "4096", "8192"};
	int resIndex = s.shadowResolution >= 8192 ? 3 : s.shadowResolution >= 4096 ? 2
												 : s.shadowResolution >= 2048 ? 1 : 0;
	if(ImGui::Combo("Shadow quality", &resIndex, resNames, 4))
		s.shadowResolution = 1024 << resIndex;
	ImGui::SliderFloat("Shadow distance", &s.shadowDistance, 48.0f, 256.0f, "%.0f blocks");
	ImGui::Checkbox("Water reflections (SSR)", &s.waterReflections);

	ImGui::SeparatorText("Post-processing");
	ImGui::SliderFloat("Exposure", &s.exposure, 0.2f, 4.0f, "%.2f");
	ImGui::Checkbox("Bloom", &s.bloom);
	ImGui::SliderFloat("Bloom strength", &s.bloomStrength, 0.0f, 0.3f, "%.3f");
	ImGui::Checkbox("God rays", &s.godRays);
	ImGui::SliderFloat("Saturation", &s.saturation, 0.0f, 2.0f, "%.2f");

	ImGui::SeparatorText("Display");
	ImGui::SliderFloat("Render scale", &s.renderScale, 0.5f, 1.0f, "%.2f");
	ImGui::Checkbox("VSync", &s.vsync);
	ImGui::PopItemWidth();

	if(ImGui::Button("Reset to defaults"))
		s = RenderSettings{};
	ImGui::End();
}
