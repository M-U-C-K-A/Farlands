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

	m_inWater = isInWater(world);
	float speed = m_inWater ? MOVE_SPEED * WATER_SPEED_SCALE : MOVE_SPEED;
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

	bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
	if(m_inWater)
		{
			// Nage : Espace fait remonter
			if(space)
				m_velocity.y = SWIM_VELOCITY;
		}
	else if(space && m_onGround)
		{
			// Saut — seulement si au sol
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
	m_inWater = isInWater(world);

	// Appliquer la gravité (réduite et freinée dans l'eau)
	if(m_inWater)
		{
			m_velocity.y += GRAVITY * WATER_GRAVITY_SCALE * deltaTime;
			m_velocity.y *= std::max(0.0f, 1.0f - 2.0f * deltaTime);
		}
	else
		{
			m_velocity.y += GRAVITY * deltaTime;
		}
	m_velocity.y = std::max(m_velocity.y, TERMINAL_VELOCITY);

	// Sous-pas pour ne jamais traverser un bloc à grande vitesse
	float dy = m_velocity.y * deltaTime;
	int steps = static_cast<int>(std::ceil(std::abs(dy) / 0.45f));
	steps = std::max(steps, 1);
	m_onGround = false;
	for(int i = 0; i < steps && m_velocity.y != 0.0f; ++i)
		moveVertical(dy / static_cast<float>(steps), world);
}

void Player::moveVertical(float dy, const World &world)
{
	glm::vec3 newPos = m_position;
	newPos.y += dy;
	if(!collidesAt(world, newPos))
		{
			m_position.y = newPos.y;
			return;
		}

	if(dy < 0.0f)
		{
			// Atterrissage : se poser exactement sur le dessus du bloc
			float top = std::floor(newPos.y) + 1.0f;
			glm::vec3 snapped = m_position;
			snapped.y = top;
			if(top <= m_position.y && !collidesAt(world, snapped))
				m_position.y = top;
			m_onGround = true;
		}
	else
		{
			// Tête contre un plafond : se coller sous le bloc
			float ceiling = std::floor(newPos.y + PLAYER_HEIGHT);
			glm::vec3 snapped = m_position;
			snapped.y = ceiling - PLAYER_HEIGHT - 0.001f;
			if(snapped.y >= m_position.y && !collidesAt(world, snapped))
				m_position.y = snapped.y;
		}
	m_velocity.y = 0.0f;
}

// ── Spawn ───────────────────────────────────────────────────────
void Player::spawnOnSurface(const World &world)
{
	int bx = static_cast<int>(std::floor(m_position.x));
	int bz = static_cast<int>(std::floor(m_position.z));
	for(int y = CHUNK_SIZE_Y - 1; y >= 0; --y)
		{
			if(world.getBlockAt(bx, y, bz) != BlockType::Air)
				{
					m_position.y = static_cast<float>(y + 1);
					break;
				}
		}
	m_velocity = glm::vec3(0.0f);
	// Sécurité : ne jamais apparaître dans un bloc solide
	while(collidesAt(world, m_position) && m_position.y < CHUNK_SIZE_Y)
		m_position.y += 1.0f;
}

bool Player::isInWater(const World &world) const
{
	// Teste au niveau des pieds et à mi-hauteur
	int bx = static_cast<int>(std::floor(m_position.x));
	int bz = static_cast<int>(std::floor(m_position.z));
	int feet = static_cast<int>(std::floor(m_position.y + 0.1f));
	int waist = static_cast<int>(std::floor(m_position.y + 0.9f));
	return world.getBlockAt(bx, feet, bz) == BlockType::Water
		   || world.getBlockAt(bx, waist, bz) == BlockType::Water;
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
							if(BlockDatabase::IsSolid(block))
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
