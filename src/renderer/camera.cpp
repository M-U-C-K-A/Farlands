// -----------------------------------------------------------------------------
// Fichier : camera.cpp
// Rôle : Implémentation des matrices de la caméra.
// -----------------------------------------------------------------------------
#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera()
	: m_position(0.0f, 80.0f, 0.0f), m_front(0.0f, 0.0f, -1.0f),
	  m_up(0.0f, 1.0f, 0.0f), m_fov(70.0f), m_aspectRatio(16.0f / 9.0f),
	  m_nearPlane(0.05f), m_farPlane(6000.0f)
{}

void Camera::setDirection(const glm::vec3 &front)
{
	m_front = glm::normalize(front);
}

glm::mat4 Camera::getViewMatrix() const
{
	return glm::lookAt(glm::vec3(0.0f), m_front, m_up);
}

glm::mat4 Camera::getProjectionMatrix() const
{
	// near/far inversés => profondeur 1 au plan proche, 0 au plan lointain
	glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(m_fov), m_aspectRatio,
										   m_farPlane, m_nearPlane);
	proj[1][1] *= -1.0f; // Vulkan : Y vers le bas
	return proj;
}

std::array<glm::vec4, 4> Camera::getFrustumPlanes() const
{
	glm::mat4 m = getProjectionMatrix() * getViewMatrix();
	glm::vec4 row0(m[0][0], m[1][0], m[2][0], m[3][0]);
	glm::vec4 row1(m[0][1], m[1][1], m[2][1], m[3][1]);
	glm::vec4 row3(m[0][3], m[1][3], m[2][3], m[3][3]);
	std::array<glm::vec4, 4> planes = {row3 + row0, row3 - row0, row3 + row1,
									   row3 - row1};
	for(auto &p : planes)
		p /= glm::length(glm::vec3(p));
	return planes;
}
