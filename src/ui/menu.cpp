#include "menu.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

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

		// Couleur indicative selon le temps de frame (vert / orange / rouge)
		ImVec4 frameColor(float ms)
		{
			if(ms <= 1000.0f / 59.0f)
				return ImVec4(0.45f, 0.9f, 0.45f, 1.0f);
			if(ms <= 1000.0f / 29.0f)
				return ImVec4(1.0f, 0.75f, 0.3f, 1.0f);
			return ImVec4(1.0f, 0.4f, 0.35f, 1.0f);
		}

		void row(const char *label, const char *fmt, ...)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", label);
			ImGui::TableNextColumn();
			char buf[256];
			va_list args;
			va_start(args, fmt);
			std::vsnprintf(buf, sizeof(buf), fmt, args);
			va_end(args);
			ImGui::TextUnformatted(buf);
		}

		bool beginInfoTable(const char *id)
		{
			return ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg);
		}
	}

void MainMenu::draw(GLFWwindow *window, ImTextureID background, ImTextureID logo,
					RenderSettings &settings, bool gameStarted, const DebugInfo &info)
{
	if(m_showOptions)
		drawOptions(settings, info);
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

	const float third = (bw - 20.0f) / 3.0f;
	ImGui::SetCursorPos(ImVec2(w * 0.5f - bw * 0.5f, y + 52.0f));
	if(bigButton("Options...", ImVec2(third, bh)))
		m_showOptions = !m_showOptions;

	ImGui::SetCursorPos(ImVec2(w * 0.5f - bw * 0.5f + third + 10.0f, y + 52.0f));
	if(bigButton("World...", ImVec2(third, bh)))
		openOptions(4);

	ImGui::SetCursorPos(ImVec2(w * 0.5f - bw * 0.5f + 2.0f * (third + 10.0f), y + 52.0f));
	if(bigButton("Quit Game", ImVec2(third, bh)))
		glfwSetWindowShouldClose(window, true);

	// Préréglage de shaders courant, sous les boutons
	ImGui::SetCursorPos(ImVec2(w * 0.5f - bw * 0.5f, y + 104.0f));
	ImGui::TextColored(ImVec4(1, 1, 1, 0.75f), "Shaders: %s   |   %.0f FPS",
					   presetName(settings.preset), info.fps);

	ImGui::SetCursorPos(ImVec2(10.0f, h - 26.0f));
	ImGui::TextColored(ImVec4(1, 1, 1, 0.6f), "Farlands - C++20 / Vulkan");
	ImGui::End();
}

void MainMenu::drawFpsCounter(const DebugInfo &info)
{
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(display.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.35f);
	ImGui::Begin("##fps", nullptr,
				 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
				   | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
				   | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
	float ms = info.fps > 0.0f ? 1000.0f / info.fps : 0.0f;
	ImGui::TextColored(frameColor(ms), "%.0f FPS", info.fps);
	ImGui::SameLine();
	ImGui::TextDisabled("%.1f ms  1%% low %.0f", ms, info.low1Fps);
	ImGui::PlotLines("##ft", info.frameMs.data(), DebugInfo::HISTORY, info.historyHead,
					 nullptr, 0.0f, std::max(20.0f, info.frameMaxMs * 1.1f), ImVec2(220, 32));
	ImGui::End();
}

void MainMenu::drawOptions(RenderSettings &s, const DebugInfo &info)
{
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(display.x - 20.0f, 20.0f), ImGuiCond_Appearing,
							ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(470.0f, std::min(display.y - 40.0f, 680.0f)),
							 ImGuiCond_Appearing);
	ImGui::SetNextWindowBgAlpha(0.88f);
	if(!ImGui::Begin("Options", &m_showOptions))
		{
			ImGui::End();
			return;
		}

	auto tabFlags = [&](int tab) {
		return m_forceTab == tab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
	};
	if(ImGui::BeginTabBar("##tabs"))
		{
			if(ImGui::BeginTabItem("Shaders", nullptr, tabFlags(0)))
				{
					drawShadersTab(s);
					ImGui::EndTabItem();
				}
			if(ImGui::BeginTabItem("Graphics", nullptr, tabFlags(1)))
				{
					drawGraphicsTab(s);
					ImGui::EndTabItem();
				}
			if(ImGui::BeginTabItem("World", nullptr, tabFlags(4)))
				{
					drawWorldTab(s, info);
					ImGui::EndTabItem();
				}
			if(ImGui::BeginTabItem("Performance", nullptr, tabFlags(2)))
				{
					drawPerformanceTab(info);
					ImGui::EndTabItem();
				}
			if(ImGui::BeginTabItem("System", nullptr, tabFlags(3)))
				{
					drawSystemTab(info);
					ImGui::EndTabItem();
				}
			ImGui::EndTabBar();
		}
	m_forceTab = -1;
	ImGui::End();
}

void MainMenu::drawShadersTab(RenderSettings &s)
{
	// ── Préréglages ─────────────────────────────────────────────
	ImGui::SeparatorText("Preset");
	for(int i = 0; i < static_cast<int>(ShaderPreset::Custom); ++i)
		{
			ShaderPreset p = static_cast<ShaderPreset>(i);
			if(i > 0)
				ImGui::SameLine();
			bool selected = s.preset == p;
			if(selected)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.45f, 0.75f, 1.0f));
			if(ImGui::Button(presetName(p), ImVec2(100, 30)))
				{
					applyPreset(s, p);
					m_presetChanged = true;
				}
			if(selected)
				ImGui::PopStyleColor();
		}
	if(s.preset == ShaderPreset::Custom)
		ImGui::TextDisabled("Custom (modified by hand)");

	// Toute modification manuelle bascule en "Custom"
	bool c = false;
	ImGui::PushItemWidth(220.0f);
	ImGui::SeparatorText("Sun & sky");
	c |= ImGui::SliderFloat("Sun elevation", &s.sunElevation, 1.0f, 89.0f, "%.1f deg");
	c |= ImGui::SliderFloat("Sun azimuth", &s.sunAzimuth, 0.0f, 360.0f, "%.0f deg");
	c |= ImGui::Checkbox("Square sun", &s.squareSun);
	ImGui::SameLine();
	c |= ImGui::Checkbox("Night (moonlight)", &s.night);
	if(s.night)
		c |= ImGui::SliderFloat("Moon elevation", &s.moonElevation, 5.0f, 85.0f, "%.0f deg");
	c |= ImGui::Checkbox("Volumetric clouds", &s.clouds);
	c |= ImGui::SliderFloat("Cloud coverage", &s.cloudCoverage, 0.0f, 1.0f, "%.2f");
	c |= ImGui::SliderFloat("Cloud density", &s.cloudDensity, 0.2f, 3.0f, "%.2f");
	c |= ImGui::SliderFloat("Cloud brightness", &s.cloudBrightness, 0.1f, 1.5f, "%.2f");

	ImGui::SeparatorText("Fog & light");
	c |= ImGui::SliderFloat("Haze", &s.fogDensity, 0.0f, 6.0f, "%.2f");
	c |= ImGui::Checkbox("Volumetric fog", &s.volumetricFog);
	c |= ImGui::SliderFloat("Mist density", &s.volumetricDensity, 0.0f, 6.0f, "%.2f");
	c |= ImGui::SliderFloat("Mist height", &s.mistHeight, 2.0f, 80.0f, "%.0f blocks");
	c |= ImGui::SliderFloat("Light shafts", &s.lightShafts, 0.0f, 4.0f, "%.2f");
	c |= ImGui::Checkbox("Screen-space god rays", &s.godRays);

	ImGui::SeparatorText("Color grading");
	c |= ImGui::SliderFloat("Exposure", &s.exposure, 0.2f, 4.0f, "%.2f");
	c |= ImGui::SliderFloat("Contrast", &s.contrast, 0.5f, 1.6f, "%.2f");
	c |= ImGui::SliderFloat("Saturation", &s.saturation, 0.0f, 2.0f, "%.2f");
	c |= ImGui::ColorEdit3("Color filter", &s.colorFilter.x, ImGuiColorEditFlags_Float);
	c |= ImGui::ColorEdit3("Shadow tint", &s.shadowTint.x, ImGuiColorEditFlags_Float);
	c |= ImGui::SliderFloat("Vignette", &s.vignette, 0.0f, 1.5f, "%.2f");
	c |= ImGui::Checkbox("Bloom", &s.bloom);
	c |= ImGui::SliderFloat("Bloom strength", &s.bloomStrength, 0.0f, 0.3f, "%.3f");
	ImGui::PopItemWidth();
	if(c)
		s.preset = ShaderPreset::Custom;
}

void MainMenu::drawGraphicsTab(RenderSettings &s)
{
	ImGui::PushItemWidth(220.0f);
	ImGui::SeparatorText("World");
	ImGui::SliderInt("Render distance", &s.renderDistance, 4, 48, "%d chunks");
	ImGui::SliderInt("Far terrain (LOD)", &s.lodDistance, 0, 6144,
					 s.lodDistance == 0 ? "off" : "%d blocks");
	ImGui::Checkbox("Falling leaves", &s.fallingLeaves);

	ImGui::SeparatorText("Lighting");
	ImGui::Checkbox("Shadows", &s.shadows);
	const char *resNames[] = {"1024", "2048", "4096", "8192"};
	int resIndex = s.shadowResolution >= 8192 ? 3 : s.shadowResolution >= 4096 ? 2
												 : s.shadowResolution >= 2048 ? 1 : 0;
	if(ImGui::Combo("Shadow quality", &resIndex, resNames, 4))
		s.shadowResolution = 1024 << resIndex;
	ImGui::SliderFloat("Shadow distance", &s.shadowDistance, 48.0f, 256.0f, "%.0f blocks");
	ImGui::Checkbox("Water reflections (SSR)", &s.waterReflections);

	ImGui::SeparatorText("Display");
	ImGui::SliderFloat("Field of view", &s.fov, 50.0f, 110.0f, "%.0f deg");
	ImGui::SliderFloat("Render scale", &s.renderScale, 0.5f, 1.0f, "%.2f");
	ImGui::Checkbox("VSync", &s.vsync);
	ImGui::Checkbox("Show FPS counter", &s.showFps);
	ImGui::TextDisabled("F11: fullscreen   F3: debug overlay   F2: screenshot");
	ImGui::PopItemWidth();

	ImGui::Spacing();
	if(ImGui::Button("Reset all settings"))
		s = RenderSettings{};
}

void MainMenu::drawPerformanceTab(const DebugInfo &info)
{
	const Renderer::Stats &r = info.render;
	float ms = info.fps > 0.0f ? 1000.0f / info.fps : 0.0f;

	// ── FPS ─────────────────────────────────────────────────────
	ImGui::SetWindowFontScale(1.6f);
	ImGui::TextColored(frameColor(ms), "%.0f FPS", info.fps);
	ImGui::SetWindowFontScale(1.0f);
	ImGui::SameLine();
	ImGui::TextDisabled("  %.2f ms avg   1%% low %.0f FPS   worst %.1f ms", info.frameAvgMs,
						info.low1Fps, info.frameMaxMs);

	char overlay[64];
	std::snprintf(overlay, sizeof(overlay), "frame time (last %d frames)", DebugInfo::HISTORY);
	ImGui::PlotLines("##frametime", info.frameMs.data(), DebugInfo::HISTORY, info.historyHead,
					 overlay, 0.0f, std::max(20.0f, info.frameMaxMs * 1.1f),
					 ImVec2(ImGui::GetContentRegionAvail().x, 80));

	// ── GPU par passe ───────────────────────────────────────────
	ImGui::SeparatorText("GPU");
	if(r.gpuMs > 0.0f)
		{
			ImGui::Text("GPU frame: %.2f ms", r.gpuFrameMs);
			for(int i = 0; i < Renderer::PASS_COUNT; ++i)
				{
					char label[64];
					std::snprintf(label, sizeof(label), "%s  %.2f ms", Renderer::passName(i),
								  r.passMs[i]);
					ImGui::ProgressBar(r.gpuMs > 0.0f ? r.passMs[i] / r.gpuMs : 0.0f,
									   ImVec2(-1.0f, 0.0f), label);
				}
			ImGui::TextDisabled("Apple GPUs overlap passes: per-pass times are approximate.");
		}
	else
		ImGui::TextDisabled("GPU timestamps not supported on this device.");

	// ── CPU ─────────────────────────────────────────────────────
	ImGui::SeparatorText("CPU (ms)");
	if(beginInfoTable("##cpu"))
		{
			row("Frame (drawFrame)", "%.2f", r.drawFrameMs);
			row("  waiting for GPU", "%.2f", r.fenceWaitMs);
			row("  command recording", "%.3f", r.recordMs);
			row("World streaming", "%.3f", info.worldUpdateMs);
			row("Mesh uploads", "%.3f", info.uploadMs);
			row("Interface", "%.3f", info.uiMs);
			ImGui::EndTable();
		}

	// ── Scène ───────────────────────────────────────────────────
	ImGui::SeparatorText("Scene");
	if(beginInfoTable("##scene"))
		{
			row("Draw calls", "%u", r.drawCalls);
			row("Triangles", "%.2f M", r.triangles / 1e6);
			row("Shadow map triangles", "%.2f M (%u updates)", r.shadowTriangles / 1e6,
				r.shadowUpdates);
			row("Chunks on GPU / visible", "%d / %d", r.chunks, r.drawnChunks);
			row("Shadow casters", "%d", r.shadowChunks);
			row("Chunk meshes memory", "%zu MB", r.gpuMemoryMB);
			row("Far terrain tiles", "%d (%d visible, %zu MB)", info.lodTiles,
				r.drawnLodTiles, r.lodMemoryMB);
			row("Falling leaves", "%d", info.particles);
			row("Render resolution", "%u x %u", r.renderExtent.width, r.renderExtent.height);
			row("Output resolution", "%u x %u", r.swapchainExtent.width, r.swapchainExtent.height);
			ImGui::EndTable();
		}

	// ── Monde ───────────────────────────────────────────────────
	ImGui::SeparatorText("World");
	if(beginInfoTable("##world"))
		{
			row("Chunks loaded / meshed", "%d / %d", info.chunksLoaded, info.chunksMeshed);
			row("Pending jobs", "%zu (%u threads)", info.pendingJobs, info.workerThreads);
			row("Render radius", "%d chunks (%d blocks)", info.renderRadius, info.renderRadius * 16);
			row("Position", "%.1f, %.1f, %.1f", info.position.x, info.position.y, info.position.z);
			row("Biome", "%s", info.biome.c_str());
			ImGui::EndTable();
		}
}

void MainMenu::drawSystemTab(const DebugInfo &info)
{
	const Renderer::DeviceInfo &d = info.device;
	ImGui::SeparatorText("Graphics");
	if(beginInfoTable("##gpu"))
		{
			row("GPU", "%s", d.gpuName.c_str());
			row("Vulkan API", "%s", d.apiVersion.c_str());
			row("Driver", "%s", d.driverVersion.c_str());
			row("Present mode", "%s", d.presentMode.c_str());
			row("Swapchain", "%s, %u images", d.swapchainFormat.c_str(), d.swapchainImages);
			row("Shadow map", "%d x %d", d.shadowResolution, d.shadowResolution);
			ImGui::EndTable();
		}
	ImGui::SeparatorText("Display");
	if(beginInfoTable("##display"))
		{
			row("Window", "%d x %d%s", info.windowW, info.windowH,
				info.fullscreen ? " (fullscreen)" : "");
			row("Monitor", "%d x %d @ %d Hz", info.monitorW, info.monitorH, info.monitorHz);
			ImGui::EndTable();
		}
	ImGui::SeparatorText("Engine");
	if(beginInfoTable("##engine"))
		{
			row("CPU threads", "%u", info.cpuThreads);
			row("World worker threads", "%u", info.workerThreads);
#ifdef NDEBUG
			row("Build", "Release");
#else
			row("Build", "Debug");
#endif
			row("Compiler", "%s", __VERSION__);
			row("Uptime", "%.0f s", info.uptime);
			row("ImGui", "%s", IMGUI_VERSION);
			ImGui::EndTable();
		}
}

void MainMenu::drawWorldTab(RenderSettings &s, const DebugInfo &info)
{
	ImGui::SeparatorText("Explore");
	ImGui::TextDisabled("Teleport (flying) to the nearest:");
	const StructureType structures[] = {StructureType::Village,    StructureType::DesertTemple,
										StructureType::Ruins,      StructureType::Igloo,
										StructureType::Shrine,     StructureType::Lighthouse};
	int i = 0;
	for(StructureType t : structures)
		{
			if(i++ % 3 != 0)
				ImGui::SameLine();
			if(ImGui::Button(structureName(t), ImVec2(136, 28)))
				{
					m_worldRequest.kind = WorldRequest::LocateStructure;
					m_worldRequest.structure = t;
				}
		}
	const BiomeType biomes[] = {BiomeType::Amplified, BiomeType::CherryGrove, BiomeType::SnowyPeaks,
								BiomeType::Desert, BiomeType::Taiga, BiomeType::Forest};
	i = 0;
	for(BiomeType b : biomes)
		{
			if(i++ % 3 != 0)
				ImGui::SameLine();
			const BiomeData &bd = getBiomeData(b);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(bd.color.r * 0.55f, bd.color.g * 0.55f,
														  bd.color.b * 0.55f, 0.85f));
			if(ImGui::Button(bd.name.c_str(), ImVec2(136, 28)))
				{
					m_worldRequest.kind = WorldRequest::LocateBiome;
					m_worldRequest.biome = b;
				}
			ImGui::PopStyleColor();
		}

	ImGui::SeparatorText("Movement");
	ImGui::SliderFloat("Fly speed", &s.flySpeed, 0.25f, 6.0f, "x%.2f");
	ImGui::TextDisabled("Double-tap Space or press F to fly; Ctrl to go faster.");

	ImGui::SeparatorText("New world");
	if(m_seedInput < 0)
		{
			m_seedInput = static_cast<int>(info.worldSeed & 0x7FFFFFFF);
			m_amplifiedInput = info.amplifiedWorld;
		}
	ImGui::InputInt("Seed", &m_seedInput);
	m_seedInput = std::max(m_seedInput, 0);
	ImGui::SameLine();
	if(ImGui::Button("Random"))
		m_seedInput = static_cast<int>((static_cast<uint32_t>(glfwGetTime() * 1e6) * 2654435761u) & 0x7FFFFFFF);
	ImGui::Checkbox("Amplified terrain everywhere", &m_amplifiedInput);
	if(ImGui::Button("Generate world", ImVec2(200, 30)))
		{
			m_worldRequest.kind = WorldRequest::NewWorld;
			m_worldRequest.seed = static_cast<uint32_t>(m_seedInput);
			m_worldRequest.amplified = m_amplifiedInput;
		}
	ImGui::TextDisabled("Current: seed %u%s", info.worldSeed, info.amplifiedWorld ? " (amplified)" : "");
}
