// -----------------------------------------------------------------------------
// Fichier : camera.h
// Rôle : Caméra de la scène 3D.
// Rendu relatif à la caméra : la matrice de vue ne contient que la rotation,
// les positions sont envoyées au GPU par rapport à la caméra (précision).
// Projection "reverse-Z" (près = 1, loin = 0) pour une profondeur précise
// jusqu'à l'horizon.
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>

#include <array>

class Camera
{
  public:
	Camera();

	// Définit la position spatiale (XYZ) de la caméra
	void setPosition(const glm::vec3 &pos) { m_position = pos; }
	// Définit la direction du regard (normalisée)
	void setDirection(const glm::vec3 &front);
	// Met à jour le ratio d'aspect selon les dimensions de la fenêtre
	void setAspectRatio(float aspect) { m_aspectRatio = aspect; }
	void setFov(float degrees) { m_fov = degrees; }

	const glm::vec3 &getPosition() const { return m_position; }
	const glm::vec3 &getFront() const { return m_front; }
	float getNear() const { return m_nearPlane; }
	float getFar() const { return m_farPlane; }

	/// Vue sans translation (positions relatives à la caméra)
	glm::mat4 getViewMatrix() const;
	/// Projection perspective reverse-Z, Y inversé pour Vulkan
	glm::mat4 getProjectionMatrix() const;

	/// Plans du frustum (espace relatif à la caméra) : ax + by + cz + d >= 0
	std::array<glm::vec4, 4> getFrustumPlanes() const;

  private:
	glm::vec3 m_position;
	glm::vec3 m_front;
	glm::vec3 m_up;
	float m_fov;
	float m_aspectRatio;
	float m_nearPlane;
	float m_farPlane;
};
