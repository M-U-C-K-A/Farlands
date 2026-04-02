#include "menu.h"
#include "imgui.h"

bool MainMenu::draw(GLFWwindow *window) {
  if (!m_active)
    return false;

  int w, h;
  glfwGetFramebufferSize(window, &w, &h);

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), static_cast<float>(h)));
  ImGui::Begin("Main Menu", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);

  ImGui::SetCursorPos(
      ImVec2(static_cast<float>(w) / 2 - 200, static_cast<float>(h) / 2 - 60));
  if (ImGui::Button("Singleplayer", ImVec2(400, 40))) {
    m_active = false;
  }

  ImGui::SetCursorPos(
      ImVec2(static_cast<float>(w) / 2 - 200, static_cast<float>(h) / 2));
  if (ImGui::Button("Options", ImVec2(350, 40))) {
  }

  ImGui::SetCursorPos(
      ImVec2(static_cast<float>(w) / 2 + 160, static_cast<float>(h) / 2));
  if (ImGui::Button("Quit", ImVec2(40, 40))) {
    glfwSetWindowShouldClose(window, true);
  }

  ImGui::End();
  return !m_active; // true = player started game
}
