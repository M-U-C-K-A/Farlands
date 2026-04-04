// -----------------------------------------------------------------------------
// Fichier : player.cpp
// Rôle : Implémentation du contrôleur joueur first-person.
// Gère WASD + souris, gravité, saut et collisions AABB complètes.
// -----------------------------------------------------------------------------
#include "player.h"

#include "../world/block.h"
#include "../world/chunk.h"
#include "../world/world.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

// Constructeur : spawn au-dessus du terrain, regard horizontal
Player::Player()
	: m_position(8.0f, 60.0f, 8.0f),
	  m_velocity(0.0f), m_yaw(-90.0f), m_pitch(0.0f), m_onGround(false)
{}

// ── Entrées clavier ─────────────────────────────────────────────
void Player::handleInput(GLFWwindow *window, float deltaTime,
						 const World &world)
{
	glm::vec3 front = getHorizontalFront();
	glm::vec3 right
	  = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

	float speed = MOVE_SPEED;
	glm::vec3 moveDir(0.0f);

	// WASD — déplacement horizontal
	if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		moveDir += front;
	if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		moveDir -= front;
	if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		moveDir -= right;
	if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		moveDir += right;

	// Normaliser pour éviter que la diagonale soit plus rapide
	if(glm::length(moveDir) > 0.001f)
		moveDir = glm::normalize(moveDir);

	float dx = moveDir.x * speed * deltaTime;
	float dz = moveDir.z * speed * deltaTime;

	// ── Collision par axe séparé ────────────────────────────
	// Axe X
	glm::vec3 newPosX = m_position;
	newPosX.x += dx;
	if(!collidesAt(world, newPosX))
		m_position.x = newPosX.x;

	// Axe Z
	glm::vec3 newPosZ = m_position;
	newPosZ.z += dz;
	if(!collidesAt(world, newPosZ))
		m_position.z = newPosZ.z;

	// Saut — seulement si au sol
	if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && m_onGround)
		{
			m_velocity.y = JUMP_VELOCITY;
			m_onGround = false;
		}
}

// ── Entrées souris ──────────────────────────────────────────────
void Player::handleMouseMove(double xOffset, double yOffset)
{
	m_yaw += static_cast<float>(xOffset) * MOUSE_SENSITIVITY;
	m_pitch -= static_cast<float>(yOffset) * MOUSE_SENSITIVITY;
	m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
}

// ── Mise à jour physique ────────────────────────────────────────
void Player::update(float deltaTime, const World &world)
{
	// Appliquer la gravité
	m_velocity.y += GRAVITY * deltaTime;

	// Déplacement vertical
	float dy = m_velocity.y * deltaTime;

	// ── Collision vers le bas ────────────────────────────────
	if(dy < 0.0f)
		{
			glm::vec3 newPos = m_position;
			newPos.y += dy;
			if(collidesAt(world, newPos))
				{
					// Trouver la surface exacte
					// On remonte jusqu'à ne plus collisionner
					float step = 0.01f;
					while(collidesAt(world, m_position)
						  && m_position.y < m_position.y + 5.0f)
						{
							m_position.y += step;
						}
					m_velocity.y = 0.0f;
					m_onGround = true;
				}
			else
				{
					m_position.y = newPos.y;
					m_onGround = false;
				}
		}
	// ── Collision vers le haut (tête) ───────────────────────
	else if(dy > 0.0f)
		{
			glm::vec3 newPos = m_position;
			newPos.y += dy;
			if(collidesAt(world, newPos))
				{
					m_velocity.y = 0.0f; // Bloquer en haut
				}
			else
				{
					m_position.y = newPos.y;
					m_onGround = false;
				}
		}
}

// ── Collision AABB ──────────────────────────────────────────────
// Vérifie si la hitbox du joueur à la position donnée chevauche un bloc solide.
// La hitbox est centrée horizontalement : [pos.x ± halfW, pos.z ± halfW]
// Et va de pos.y à pos.y + PLAYER_HEIGHT verticalement.
bool Player::collidesAt(const World &world, const glm::vec3 &pos) const
{
	float halfW = PLAYER_WIDTH * 0.5f;

	// AABB du joueur
	float minX = pos.x - halfW;
	float maxX = pos.x + halfW;
	float minY = pos.y;
	float maxY = pos.y + PLAYER_HEIGHT;
	float minZ = pos.z - halfW;
	float maxZ = pos.z + halfW;

	// Itérer sur tous les blocs couverts par l'AABB
	int bMinX = static_cast<int>(std::floor(minX));
	int bMaxX = static_cast<int>(std::floor(maxX));
	int bMinY = static_cast<int>(std::floor(minY));
	int bMaxY = static_cast<int>(std::floor(maxY));
	int bMinZ = static_cast<int>(std::floor(minZ));
	int bMaxZ = static_cast<int>(std::floor(maxZ));

	for(int bx = bMinX; bx <= bMaxX; ++bx)
		{
			for(int by = bMinY; by <= bMaxY; ++by)
				{
					for(int bz = bMinZ; bz <= bMaxZ; ++bz)
						{
							BlockType block = world.getBlockAt(bx, by, bz);
							if(block != BlockType::Air)
								{
									// Le bloc occupe [bx, bx+1] × [by, by+1]
									// × [bz, bz+1] Vérifier le chevauchement
									// AABB
									if(maxX > static_cast<float>(bx)
									   && minX < static_cast<float>(bx + 1)
									   && maxY > static_cast<float>(by)
									   && minY < static_cast<float>(by + 1)
									   && maxZ > static_cast<float>(bz)
									   && minZ < static_cast<float>(bz + 1))
										{
											return true;
										}
								}
						}
				}
		}

	return false;
}

// ── Accesseurs ──────────────────────────────────────────────────
glm::vec3 Player::getEyePosition() const
{
	return m_position + glm::vec3(0.0f, PLAYER_HEIGHT, 0.0f);
}

glm::vec3 Player::getFrontVector() const
{
	glm::vec3 front;
	front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	front.y = sin(glm::radians(m_pitch));
	front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
	return glm::normalize(front);
}

glm::vec3 Player::getHorizontalFront() const
{
	glm::vec3 front;
	front.x = cos(glm::radians(m_yaw));
	front.y = 0.0f;
	front.z = sin(glm::radians(m_yaw));
	return glm::normalize(front);
}

int Player::getChunkX() const
{
	int wx = static_cast<int>(std::floor(m_position.x));
	return (wx >= 0) ? (wx / CHUNK_SIZE_X)
					 : ((wx - CHUNK_SIZE_X + 1) / CHUNK_SIZE_X);
}

int Player::getChunkZ() const
{
	int wz = static_cast<int>(std::floor(m_position.z));
	return (wz >= 0) ? (wz / CHUNK_SIZE_Z)
					 : ((wz - CHUNK_SIZE_Z + 1) / CHUNK_SIZE_Z);
}
