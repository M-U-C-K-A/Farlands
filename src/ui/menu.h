// -----------------------------------------------------------------------------
// Fichier : menu.h
// Rôle : Menu principal (par-dessus le monde rendu en direct) et fenêtre
// d'options graphiques.
// -----------------------------------------------------------------------------
#pragma once

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "renderer/render_settings.h"

/// Gère l'état et le rendu du menu principal via ImGui.
class MainMenu
{
  public:
	/// Dessine le menu (si actif) et la fenêtre d'options (si ouverte).
	/// background : image de fond (0 = monde visible derrière le menu).
	void draw(GLFWwindow *window, ImTextureID background, ImTextureID logo,
			  RenderSettings &settings, bool gameStarted);

	// Indique si le menu est actuellement actif ou non
	bool isActive() const { return m_active; }
	// Définit l'état (actif ou caché) du menu principal
	void setActive(bool active) { m_active = active; }

	bool optionsOpen() const { return m_showOptions; }

  private:
	bool m_active = true;
	bool m_showOptions = false;

	void drawOptions(RenderSettings &settings);
};
