// -----------------------------------------------------------------------------
// Fichier : pipelines.h
// Rôle : Création des render passes et des pipelines graphiques.
// Tous les pipelines partagent un même pipeline layout :
//   set 0 = ressources globales de la frame, set 1 = texture d'entrée (post),
//   push constants de 32 octets.
// -----------------------------------------------------------------------------
#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

struct AttachmentDesc
{
	VkFormat format;
	VkAttachmentLoadOp load;
	VkAttachmentStoreOp store;
	VkImageLayout initialLayout;
	VkImageLayout finalLayout;
};

enum class BlendMode
{
	None,
	Alpha,
	Additive,
};

struct PipelineDesc
{
	std::string vert; // nom du fichier SPIR-V (sans dossier)
	std::string frag;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	uint32_t colorCount = 1;
	bool chunkVertices = false;     // entrée = Vertex des chunks
	bool particleInstances = false; // entrée = ParticleInstance (par instance)
	bool lodVertices = false;       // entrée = LodVertex (terrain lointain)
	VkCullModeFlags cull = VK_CULL_MODE_NONE;
	bool depthTest = false;
	bool depthWrite = false;
	VkCompareOp depthOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
	BlendMode blend = BlendMode::None;
	bool depthBias = false;
	float depthBiasConstant = 0.0f;
	float depthBiasSlope = 0.0f;
};

namespace pipelines
	{
		/// Render pass à un seul subpass, avec dépendances externes larges
		/// (les passes se lisent les unes les autres en échantillonnage).
		VkRenderPass createRenderPass(VkDevice device,
									  const std::vector<AttachmentDesc> &colors,
									  const AttachmentDesc *depth);

		VkPipeline createPipeline(VkDevice device, VkPipelineLayout layout,
								  const PipelineDesc &desc);
	}
