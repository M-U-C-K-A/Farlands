// -----------------------------------------------------------------------------
// Fichier : types.h
// Rôle : Définitions globales des types partagés CPU / GPU.
// Contient le Vertex compact des chunks, les uniformes par frame et les
// push constants. Les layouts doivent correspondre à shaders/common.glsl.
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>

// ── Vertex compact (20 octets) ──────────────────────────────────
// pos  : position locale au chunk (x, z dans [0, 16], y dans [0, 256])
// data : bits [0..9]   couche de texture
//        bits [10..12] normale (0 +Z, 1 -Z, 2 +Y, 3 -Y, 4 +X, 5 -X, 6 plante)
//        bits [13..14] occlusion ambiante (0 = aucune, 3 = maximale)
//        bit  15       u, bit 16 v (coin de texture)
//        bits [17..24] type de bloc
//        bits [25..26] ondulation (0 aucune, 1 feuillage, 2 haut de plante)
//        bits [27..28] mélange de surface (0 aucun, 1 herbe, 2 sable, 3 neige)
//        bits [29..30] blocs voisins du type mélangé autour du coin (0..3)
// tint : couleur multiplicative RGBA8 encodée sur [0, 2] (0x80 = 1.0), pour
//        pouvoir aussi éclaircir/saturer (teinte de biome)
struct Vertex
{
	glm::vec3 pos;
	uint32_t data;
	uint32_t tint;

	static uint32_t pack(uint32_t layer, uint32_t normal, uint32_t ao,
						 uint32_t u, uint32_t v, uint32_t blockType,
						 uint32_t wave, uint32_t blendKind = 0,
						 uint32_t blendCount = 0)
	{
		return (layer & 0x3FFu) | ((normal & 7u) << 10) | ((ao & 3u) << 13)
			   | ((u & 1u) << 15) | ((v & 1u) << 16)
			   | ((blockType & 0xFFu) << 17) | ((wave & 3u) << 25)
			   | ((blendKind & 3u) << 27) | ((blendCount & 3u) << 29);
	}

	// Fournit à Vulkan la description de la structure globale du Vertex
	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription desc{};
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return desc;
	}

	// Fournit à Vulkan les détails de chaque attribut du Vertex
	static std::array<VkVertexInputAttributeDescription, 3>
	getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attrs{};
		attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)};
		attrs[1] = {1, 0, VK_FORMAT_R32_UINT, offsetof(Vertex, data)};
		attrs[2] = {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Vertex, tint)};
		return attrs;
	}
};
static_assert(sizeof(Vertex) == 20, "Vertex must stay tightly packed");

/// Teinte neutre (multiplicateur 1.0) dans l'encodage [0, 2]
constexpr uint32_t TINT_NEUTRAL = 0xFF808080u;

// ── Uniformes par frame (std140, voir shaders/common.glsl) ──────
struct FrameUniforms
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewProj;
	glm::mat4 invView;
	glm::mat4 invProj;
	glm::mat4 invViewProj;
	glm::mat4 lightViewProj; // matrice de la shadow map (soleil)

	glm::vec4 cameraPos;   // xyz : position caméra, w : temps (s)
	glm::vec4 sunDir;      // xyz : direction VERS le soleil, w : intensité du disque
	glm::vec4 sunColor;    // rgb : éclairement direct du soleil (transmis)
	glm::vec4 fogParams;   // x : densité, y : décroissance en altitude,
						   // z : distance de rendu, w : 1 si sous l'eau
	glm::vec4 waterFog;    // rgb : couleur de l'eau, a : densité sous l'eau
	glm::vec4 screen;      // xy : taille du rendu, zw : 1 / taille
	glm::vec4 post;        // x : exposition, y : bloom, z : god rays,
						   // w : saturation
	glm::vec4 clouds;      // x : couverture, y : densité, z : vitesse,
						   // w : activé
	glm::vec4 shadow;      // x : 1/taille shadow map, y : distance,
						   // z : biais (normal offset), w : activé
	glm::vec4 misc;        // x : sortie sRGB matérielle (1) ou gamma manuel,
						   // y : near, z : far, w : index de frame
	glm::vec4 blendLayers; // couches de texture : x herbe, y sable, z neige
	glm::vec4 volumetric;  // x densité de la brume, y hauteur, z rayons, w distance max
	glm::vec4 grade;       // x contraste, y vignettage, z luminosité des nuages,
						   // w ciel couvert (0..1)
	glm::vec4 colorFilter; // rgb filtre de couleur, w soleil carré (1)
	glm::vec4 shadowTint;  // rgb teinte des tons sombres
};

// ── Push constants (32 octets, communs à tous les pipelines) ────
struct PushConstants
{
	glm::vec4 a; // terrain : origine du chunk ; post : paramètres
	glm::vec4 b;
};
