#pragma once

#include <GLFW/glfw3.h>

/// Gère l'état et le rendu du menu principal via ImGui.
class MainMenu {
public:
  /// Dessine le menu ImGui. Retourne true si le joueur clique "Singleplayer".
  bool draw(GLFWwindow *window);

  bool isActive() const { return m_active; }
  void setActive(bool active) { m_active = active; }

private:
  bool m_active = true;
};
