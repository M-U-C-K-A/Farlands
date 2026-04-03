// -----------------------------------------------------------------------------
// Fichier : menu.h
// Rôle : Définition de la topologie de l'interface ImGui pour le menu
// principal.
// -----------------------------------------------------------------------------
#pragma once

#include <GLFW/glfw3.h>

/// Gère l'état et le rendu du menu principal via ImGui.
class MainMenu
{
  public:
	/// Dessine le menu ImGui. Retourne true si le joueur clique "Singleplayer".
	bool draw(GLFWwindow *window);

	// Indique si le menu est actuellement actif ou non
	bool isActive() const { return m_active; }
	// Définit l'état (actif ou caché) du menu principal
	void setActive(bool active) { m_active = active; }

  private:
	bool m_active = true;
};
