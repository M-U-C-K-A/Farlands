// -----------------------------------------------------------------------------
// Fichier : player.h
// Rôle : Contrôleur joueur first-person (FPS).
// Gère les entrées clavier/souris, la gravité, les collisions AABB complètes.
// -----------------------------------------------------------------------------
#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// Forward declaration
class World;

class Player
{
  public:
	Player();

	// ── Entrées ─────────────────────────────────────────────────
	/// Traite les entrées clavier (WASD, Space)
	void handleInput(GLFWwindow *window, float deltaTime, const World &world);
	/// Traite le mouvement de la souris (rotation caméra FPS)
	void handleMouseMove(double xOffset, double yOffset);

	// ── Mise à jour physique ────────────────────────────────────
	/// Applique la gravité, met à jour la position, résout les collisions
	void update(float deltaTime, const World &world);

	/// Téléporte le joueur (vitesse remise à zéro)
	void setPosition(const glm::vec3 &pos)
	{
		m_position = pos;
		m_velocity = glm::vec3(0.0f);
	}

	/// Place le joueur sur la surface du terrain à sa position (x, z)
	void spawnOnSurface(const World &world);

	// ── Accesseurs ──────────────────────────────────────────────
	glm::vec3 getEyePosition() const;
	glm::vec3 getFrontVector() const;
	glm::vec3 getHorizontalFront() const;

	bool isOnGround() const { return m_onGround; }
	bool isInWater() const { return m_inWater; }
	glm::vec3 getPosition() const { return m_position; }
	glm::vec3 getVelocity() const { return m_velocity; }
	float getYaw() const { return m_yaw; }
	float getPitch() const { return m_pitch; }

	/// Retourne les coordonnées chunk du joueur
	int getChunkX() const;
	int getChunkZ() const;

  private:
	glm::vec3 m_position;
	glm::vec3 m_velocity;
	float m_yaw;
	float m_pitch;
	bool m_onGround;
	bool m_inWater = false;

	// ── Constantes physiques ────────────────────────────────────
	static constexpr float GRAVITY = -20.0f;
	static constexpr float JUMP_VELOCITY = 8.0f;
	static constexpr float TERMINAL_VELOCITY = -50.0f;
	static constexpr float SWIM_VELOCITY = 4.0f;
	static constexpr float WATER_GRAVITY_SCALE = 0.25f;
	static constexpr float WATER_SPEED_SCALE = 0.6f;
	static constexpr float MOVE_SPEED = 6.0f;
	static constexpr float MOUSE_SENSITIVITY = 0.1f;
	static constexpr float PLAYER_HEIGHT = 1.8f;
	static constexpr float PLAYER_WIDTH = 0.6f;

	// ── Collision AABB ──────────────────────────────────────────
	/// Teste si l'AABB du joueur à la position donnée chevauche un bloc solide
	bool collidesAt(const World &world, const glm::vec3 &pos) const;
	/// Teste si le corps du joueur est dans l'eau
	bool isInWater(const World &world) const;
	/// Déplace verticalement de dy en résolvant les collisions
	void moveVertical(float dy, const World &world);
};
