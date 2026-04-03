// -----------------------------------------------------------------------------
// Fichier : camera.cpp
// Rôle : Implémentation des mécanismes de calcule matriciel pour la Caméra.
// -----------------------------------------------------------------------------
#include "camera.h"

// Constructeur configurant une perspective par défaut.
Camera::Camera()
	: m_position(0.0f, 60.0f, 60.0f), m_target(0.0f, 10.0f, 0.0f),
	  m_up(0.0f, 1.0f, 0.0f), m_fov(45.0f), m_aspectRatio(16.0f / 9.0f),
	  m_nearPlane(0.1f), m_farPlane(500.0f)
{}

// Change la coordonnée spatiale de la caméra
void Camera::setPosition(const glm::vec3 &pos) { m_position = pos; }

// Modifie la cible regardée par la caméra (nécessaire au calcul LookAt)
void Camera::setTarget(const glm::vec3 &target) { m_target = target; }

// Met à jour la proportion d'affichage, utile lors d'un redimensionnement
// (resize) de la fenêtre
void Camera::setAspectRatio(float aspect) { m_aspectRatio = aspect; }

// Produit la View Matrix en fonction de la position, la cible, et l'axe haut
// "up"
glm::mat4 Camera::getViewMatrix() const
{
	return glm::lookAt(m_position, m_target, m_up);
}

// Produit la Projection Matrix. Note : corrige l'axe Y pour la compatibilité
// Vulkan.
glm::mat4 Camera::getProjectionMatrix() const
{
	auto proj = glm::perspective(glm::radians(m_fov), m_aspectRatio,
								 m_nearPlane, m_farPlane);
	// Vulkan has inverted Y compared to OpenGL
	proj[1][1] *= -1;
	return proj;
}
