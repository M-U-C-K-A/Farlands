#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
  Camera();

  void setPosition(const glm::vec3 &pos);
  void setTarget(const glm::vec3 &target);
  void setAspectRatio(float aspect);

  glm::mat4 getViewMatrix() const;
  glm::mat4 getProjectionMatrix() const;

private:
  glm::vec3 m_position;
  glm::vec3 m_target;
  glm::vec3 m_up;
  float m_fov;
  float m_aspectRatio;
  float m_nearPlane;
  float m_farPlane;
};
