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
void Player::setFlying(bool flying)
{
	if(flying == m_flying)
		return;
	m_flying = flying;
	m_flyToggled = true;
	m_flyVelocity = glm::vec3(0.0f);
	m_velocity.y = 0.0f;
}

void Player::handleInput(GLFWwindow *window, float deltaTime,
						 const World &world)
{
	glm::vec3 front = getHorizontalFront();
	glm::vec3 right
	  = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
	auto down = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };

	m_inWater = isInWater(world);
	glm::vec3 moveDir(0.0f);

	// WASD — déplacement horizontal
	bool forward = down(GLFW_KEY_W);
	if(forward)
		moveDir += front;
	if(down(GLFW_KEY_S))
		moveDir -= front;
	if(down(GLFW_KEY_A))
		moveDir -= right;
	if(down(GLFW_KEY_D))
		moveDir += right;

	// Normaliser pour éviter que la diagonale soit plus rapide
	if(glm::length(moveDir) > 0.001f)
		moveDir = glm::normalize(moveDir);

	// ── Vol : double appui sur Espace, ou F ─────────────────
	bool space = down(GLFW_KEY_SPACE);
	if(space && !m_spaceWasDown)
		{
			double now = glfwGetTime();
			if(now - m_lastSpacePress < DOUBLE_TAP)
				{
					setFlying(!m_flying);
					m_lastSpacePress = -1.0;
				}
			else
				m_lastSpacePress = now;
		}
	m_spaceWasDown = space;
	bool flyKey = down(GLFW_KEY_F);
	if(flyKey && !m_flyKeyWasDown)
		setFlying(!m_flying);
	m_flyKeyWasDown = flyKey;

	bool ctrl = down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL);
	m_sprinting = ctrl && forward && !m_inWater;

	if(m_flying)
		{
			// Vitesse cible lissée (accélération / freinage progressifs)
			float speed = FLY_SPEED * m_flySpeedMul * (ctrl ? FLY_SPRINT_SCALE : 1.0f);
			glm::vec3 target = moveDir * speed;
			if(space)
				target.y += FLY_VERTICAL * m_flySpeedMul * (ctrl ? 1.8f : 1.0f);
			if(down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT))
				target.y -= FLY_VERTICAL * m_flySpeedMul * (ctrl ? 1.8f : 1.0f);
			float k = 1.0f - std::exp(-deltaTime * 9.0f);
			m_flyVelocity += (target - m_flyVelocity) * k;
			moveHorizontal(m_flyVelocity.x * deltaTime, m_flyVelocity.z * deltaTime, world);
			m_velocity.y = m_flyVelocity.y;
			return;
		}

	float speed = MOVE_SPEED;
	if(m_inWater)
		speed *= WATER_SPEED_SCALE;
	else if(m_sprinting)
		speed *= SPRINT_SCALE;
	moveHorizontal(moveDir.x * speed * deltaTime, moveDir.z * speed * deltaTime, world);

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

void Player::moveHorizontal(float dx, float dz, const World &world)
{
	float len = std::max(std::abs(dx), std::abs(dz));
	int steps = std::max(1, static_cast<int>(std::ceil(len / 0.45f)));
	float sx = dx / steps, sz = dz / steps;
	for(int i = 0; i < steps; ++i)
		{
			// Collision par axe séparé (glisse le long des murs)
			glm::vec3 p = m_position;
			p.x += sx;
			if(!collidesAt(world, p))
				m_position.x = p.x;
			else if(m_flying)
				m_flyVelocity.x = 0.0f;
			p = m_position;
			p.z += sz;
			if(!collidesAt(world, p))
				m_position.z = p.z;
			else if(m_flying)
				m_flyVelocity.z = 0.0f;
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

	// Vol : pas de gravité ; se poser au sol en descendant arrête le vol
	if(m_flying)
		{
			float dy = m_velocity.y * deltaTime;
			int steps = std::max(1, static_cast<int>(std::ceil(std::abs(dy) / 0.45f)));
			m_onGround = false;
			for(int i = 0; i < steps && m_velocity.y != 0.0f; ++i)
				moveVertical(dy / static_cast<float>(steps), world);
			if(m_velocity.y == 0.0f)
				m_flyVelocity.y = 0.0f;
			if(m_onGround && dy < 0.0f)
				setFlying(false);
			return;
		}

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
void Player::spawnOnSurface(const World &world, int startY)
{
	int bx = static_cast<int>(std::floor(m_position.x));
	int bz = static_cast<int>(std::floor(m_position.z));
	if(startY >= 0)
		m_position.y = static_cast<float>(startY);
	else
		for(int y = CHUNK_SIZE_Y - 1; y >= 0; --y)
			{
				if(BlockDatabase::IsSolid(world.getBlockAt(bx, y, bz)))
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
	return m_position + glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
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
	return floorDiv(static_cast<int>(std::floor(m_position.x)), CHUNK_SIZE_X);
}

int Player::getChunkZ() const
{
	return floorDiv(static_cast<int>(std::floor(m_position.z)), CHUNK_SIZE_Z);
}
