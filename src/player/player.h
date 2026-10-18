// -----------------------------------------------------------------------------
// Fichier : player.h
// Rôle : Contrôleur joueur first-person (FPS).
// Gère les entrées clavier/souris, la gravité, les collisions AABB complètes,
// le sprint (Ctrl) et le vol créatif (double Espace ou F).
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

	/// Place le joueur sur le terrain à sa position (x, z). startY >= 0 :
	/// part de cette hauteur (sinon du plus haut bloc) puis remonte tant
	/// que le joueur est dans un bloc solide.
	void spawnOnSurface(const World &world, int startY = -1);

	/// Oriente le regard (degrés)
	void setLook(float yaw, float pitch)
	{
		m_yaw = yaw;
		m_pitch = pitch;
	}

	// ── Accesseurs ──────────────────────────────────────────────
	glm::vec3 getEyePosition() const;
	glm::vec3 getFrontVector() const;
	glm::vec3 getHorizontalFront() const;

	bool isOnGround() const { return m_onGround; }
	bool isFlying() const { return m_flying; }
	void setFlying(bool flying);
	/// Sprint en cours (Ctrl + avancer) : élargit le champ de vision
	bool isSprinting() const { return m_sprinting; }
	/// Multiplicateur de la vitesse de vol (réglage)
	void setFlySpeed(float mul) { m_flySpeedMul = mul; }
	/// Le mode vol vient de changer (pour une notification) ; remis à faux
	bool consumeFlyToggled()
	{
		bool t = m_flyToggled;
		m_flyToggled = false;
		return t;
	}
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
	bool m_flying = false;
	bool m_flyToggled = false;
	bool m_sprinting = false;
	float m_flySpeedMul = 1.0f;
	glm::vec3 m_flyVelocity{0.0f}; // vitesse lissée en vol
	bool m_spaceWasDown = false;
	bool m_flyKeyWasDown = false;
	double m_lastSpacePress = -1.0;

	// ── Constantes physiques ────────────────────────────────────
	static constexpr float GRAVITY = -20.0f;
	static constexpr float JUMP_VELOCITY = 8.0f;
	static constexpr float TERMINAL_VELOCITY = -50.0f;
	static constexpr float SWIM_VELOCITY = 4.0f;
	static constexpr float WATER_GRAVITY_SCALE = 0.25f;
	static constexpr float WATER_SPEED_SCALE = 0.6f;
	static constexpr float MOVE_SPEED = 6.0f;
	static constexpr float SPRINT_SCALE = 1.45f;
	static constexpr float FLY_SPEED = 14.0f;       // horizontal (blocs/s)
	static constexpr float FLY_VERTICAL = 10.0f;    // montée / descente
	static constexpr float FLY_SPRINT_SCALE = 3.0f; // Ctrl en vol
	static constexpr float DOUBLE_TAP = 0.3f;       // secondes
	static constexpr float MOUSE_SENSITIVITY = 0.1f;
	static constexpr float PLAYER_HEIGHT = 1.8f;
	static constexpr float EYE_HEIGHT = 1.62f;
	static constexpr float PLAYER_WIDTH = 0.6f;

	// ── Collision AABB ──────────────────────────────────────────
	/// Teste si l'AABB du joueur à la position donnée chevauche un bloc solide
	bool collidesAt(const World &world, const glm::vec3 &pos) const;
	/// Teste si le corps du joueur est dans l'eau
	bool isInWater(const World &world) const;
	/// Déplace verticalement de dy en résolvant les collisions
	void moveVertical(float dy, const World &world);
	/// Déplace horizontalement (axes séparés, sous-pas anti-traversée)
	void moveHorizontal(float dx, float dz, const World &world);
};
