#include "camera.h"

Camera::Camera()
    : m_position(2.0f, 2.0f, 2.0f), m_target(0.0f, 0.0f, 0.0f),
      m_up(0.0f, 1.0f, 0.0f), m_fov(45.0f), m_aspectRatio(16.0f / 9.0f),
      m_nearPlane(0.1f), m_farPlane(500.0f) {}

void Camera::setPosition(const glm::vec3 &pos) { m_position = pos; }

void Camera::setTarget(const glm::vec3 &target) { m_target = target; }

void Camera::setAspectRatio(float aspect) { m_aspectRatio = aspect; }

glm::mat4 Camera::getViewMatrix() const {
  return glm::lookAt(m_position, m_target, m_up);
}

glm::mat4 Camera::getProjectionMatrix() const {
  auto proj = glm::perspective(glm::radians(m_fov), m_aspectRatio,
                               m_nearPlane, m_farPlane);
  // Vulkan has inverted Y compared to OpenGL
  proj[1][1] *= -1;
  return proj;
}
