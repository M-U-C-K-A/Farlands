// -----------------------------------------------------------------------------
// Fichier : menu.h
// Rôle : Menu principal (par-dessus le monde rendu en direct), fenêtre
// d'options (shaders, graphismes, performances, système) et compteur de FPS.
// -----------------------------------------------------------------------------
#pragma once

#include <GLFW/glfw3.h>

#include "debug_info.h"
#include "imgui.h"
#include "renderer/render_settings.h"

/// Gère l'état et le rendu du menu principal via ImGui.
class MainMenu
{
  public:
	/// Dessine le menu (si actif) et la fenêtre d'options (si ouverte).
	/// background : image de fond (0 = monde visible derrière le menu).
	void draw(GLFWwindow *window, ImTextureID background, ImTextureID logo,
			  RenderSettings &settings, bool gameStarted, const DebugInfo &info);

	/// Petit compteur de FPS (coin supérieur droit)
	static void drawFpsCounter(const DebugInfo &info);

	// Indique si le menu est actuellement actif ou non
	bool isActive() const { return m_active; }
	// Définit l'état (actif ou caché) du menu principal
	void setActive(bool active) { m_active = active; }

	bool optionsOpen() const { return m_showOptions; }
	/// Ouvre la fenêtre d'options sur un onglet (0 Shaders, 1 Graphics,
	/// 2 Performance, 3 System)
	void openOptions(int tab)
	{
		m_showOptions = true;
		m_forceTab = tab;
	}

	/// Le préréglage a été choisi dans le menu (l'application peut ajuster
	/// le soleil face au joueur) ; remis à faux après lecture.
	bool consumePresetChanged()
	{
		bool c = m_presetChanged;
		m_presetChanged = false;
		return c;
	}

  private:
	bool m_active = true;
	bool m_showOptions = false;
	bool m_presetChanged = false;
	int m_forceTab = -1;

	void drawOptions(RenderSettings &settings, const DebugInfo &info);
	void drawShadersTab(RenderSettings &s);
	void drawGraphicsTab(RenderSettings &s);
	void drawPerformanceTab(const DebugInfo &info);
	void drawSystemTab(const DebugInfo &info);
};
