// -----------------------------------------------------------------------------
// Fichier : types.h
// Rôle : Définitions globales des types essentiels pour le moteur Vulkan.
// Contient Vertex, UniformBufferObject, et leurs descriptions.
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <array>

// ── Vertex ──────────────────────────────────────────────────────
struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	// Fournit à Vulkan la description de la structure globale du Vertex
	// (taille statique, etc)
	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription desc{};
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return desc;
	}

	// Fournit à Vulkan les détails de chaque attribut du Vertex (position,
	// couleur, offset en VRAM)
	static std::array<VkVertexInputAttributeDescription, 3>
	getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attrs{};
		// position
		attrs[0].binding = 0;
		attrs[0].location = 0;
		attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrs[0].offset = offsetof(Vertex, pos);
		// color
		attrs[1].binding = 0;
		attrs[1].location = 1;
		attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrs[1].offset = offsetof(Vertex, color);
		// texCoord
		attrs[2].binding = 0;
		attrs[2].location = 2;
		attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
		attrs[2].offset = offsetof(Vertex, texCoord);
		return attrs;
	}
};

// ── Uniform Buffer Object ───────────────────────────────────────
struct UniformBufferObject
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};
