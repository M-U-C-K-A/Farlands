// -----------------------------------------------------------------------------
// Fichier : camera.h
// Rôle : Entité Caméra pour la scène 3D.
// Gère la configuration des matrices de vue (LookAt) et de projection
// perspective.
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
  public:
	// Constructeur : initialise la caméra avec des paramètres par défaut
	Camera();

	// Définit la position spatiale (XYZ) de la caméra
	void setPosition(const glm::vec3 &pos);
	// Définit le point vers lequel la caméra regarde
	void setTarget(const glm::vec3 &target);
	// Met à jour le ratio d'aspect selon les dimensions de la fenêtre
	void setAspectRatio(float aspect);

	// Calcule et retourne la matrice de Vue (View matrix) pour Vulkan
	glm::mat4 getViewMatrix() const;
	// Calcule et retourne la matrice de Projection perspective pour Vulkan
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
