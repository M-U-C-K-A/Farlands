// -----------------------------------------------------------------------------
// Fichier : renderer.h
// Rôle : Orchestrateur du rendu Vulkan. Chaîne de passes par frame :
//   1. Ciel : LUT d'atmosphère (Rayleigh/Mie) + environnement avec nuages
//   2. Ombres : shadow map du soleil (orthographique, recalée sur les texels)
//   3. Scène : terrain HDR (normal maps, ombres douces, brume) + ciel complet
//      (nuages volumétriques) -> couleur + profondeur linéaire
//   4. Eau : réflexions écran (SSR), réfraction, absorption, reflets du soleil
//   5. Bloom : chaîne de mips (réduction / agrandissement)
//   6. Final : god rays, exposition, tonemapping ACES, interface ImGui
// -----------------------------------------------------------------------------
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../core/types.h"
#include "../world/chunk_mesh.h"
#include "../world/leaf_particles.h"
#include "../world/lod_terrain.h"
#include "camera.h"
#include "render_settings.h"
#include "textures.h"
#include "vk_context.h"
#include "vk_swapchain.h"
#include "vk_utils.h"

#include "imgui.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

/// Paramètres d'une frame fournis par l'application
struct FrameParams
{
	const Camera *camera = nullptr;
	float time = 0.0f;
	bool underwater = false;
	float viewDistance = 320.0f; // rayon réellement chargé (blocs) : brume de bord
	float lodInnerRadius = 0.0f; // en deçà, le terrain lointain est masqué
};

class Renderer
{
  public:
	void init(GLFWwindow *window, const RenderSettings &settings);
	// Démarre une nouvelle frame ImGui pour construire l'UI
	void beginImGuiFrame();
	// Rend la frame courante (scène + interface) et la présente
	void drawFrame(const FrameParams &params, const RenderSettings &settings);

	// ── Meshes des chunks ───────────────────────────────────────
	void setChunkMesh(const ChunkMeshData &mesh);
	void removeChunkMesh(const glm::ivec2 &pos);

	// ── Terrain lointain (LOD) ──────────────────────────────────
	void setLodMesh(const LodMesh &mesh);
	void removeLodMesh(const glm::ivec2 &tile);
	/// Retire tous les meshes (changement de monde)
	void clearWorldMeshes();

	// ── Particules (feuilles qui tombent) ───────────────────────
	void setParticles(const std::vector<ParticleInstance> &instances);

	// Demande une capture d'écran PNG de la prochaine frame présentée
	void requestScreenshot(const std::string &path);

	void cleanup();
	void waitIdle();

	bool framebufferResized = false;

	VkDescriptorSet getPanoramaTextureId() const { return m_panoramaId; }
	VkDescriptorSet getLogoTextureId() const { return m_logoId; }
	/// Icône (texture de face) d'un bloc pour la barre d'inventaire
	ImTextureID getBlockIcon(BlockType type) const
	{
		return (ImTextureID)m_blockIcons[static_cast<uint8_t>(type)];
	}

	/// Passes chronométrées sur le GPU
	enum Pass
	{
		PASS_SKY = 0,
		PASS_SHADOW,
		PASS_SCENE,
		PASS_WATER,
		PASS_FOG,
		PASS_BLOOM,
		PASS_FINAL,
		PASS_COUNT
	};
	static const char *passName(int pass);

	struct Stats
	{
		int chunks = 0;
		int drawnChunks = 0;
		int shadowChunks = 0;
		size_t gpuMemoryMB = 0;
		size_t lodMemoryMB = 0;
		int lodTiles = 0;
		int drawnLodTiles = 0;
		float passMs[PASS_COUNT] = {};
		float gpuMs = 0.0f;
		float gpuFrameMs = 0.0f;  // premier -> dernier timestamp
		float drawFrameMs = 0.0f; // CPU : drawFrame complet
		float fenceWaitMs = 0.0f; // CPU : attente du GPU
		float recordMs = 0.0f;    // CPU : enregistrement des commandes
		uint32_t drawCalls = 0;
		uint64_t triangles = 0;
		uint64_t shadowTriangles = 0;
		uint32_t shadowUpdates = 0; // recalculs de la shadow map (cumul)
		VkExtent2D renderExtent{0, 0};
		VkExtent2D swapchainExtent{0, 0};
	};

	/// Informations sur le GPU et le swapchain (onglet Système)
	struct DeviceInfo
	{
		std::string gpuName;
		std::string apiVersion;
		std::string driverVersion;
		std::string presentMode;
		std::string swapchainFormat;
		uint32_t swapchainImages = 0;
		int shadowResolution = 0;
	};
	DeviceInfo getDeviceInfo() const;
	const Stats &getStats() const { return m_stats; }

  private:
	// ── Mesh GPU d'un chunk (un buffer : sommets puis indices) ──
	struct GpuChunk
	{
		GpuBuffer buffer;
		VkDeviceSize indexOffset = 0;
		uint32_t opaqueCount = 0;
		uint32_t cutoutCount = 0;
		uint32_t waterCount = 0;
		glm::ivec2 pos{0};
		float minY = 0.0f, maxY = 0.0f;
	};

	GLFWwindow *m_window = nullptr;
	VkContext m_context;
	VkSwapchain m_swapchain;

	// ── Render passes ───────────────────────────────────────────
	VkRenderPass m_rpSky = VK_NULL_HANDLE;
	VkRenderPass m_rpShadow = VK_NULL_HANDLE;
	VkRenderPass m_rpScene = VK_NULL_HANDLE;
	VkRenderPass m_rpWater = VK_NULL_HANDLE;
	VkRenderPass m_rpBloomDown = VK_NULL_HANDLE;
	VkRenderPass m_rpBloomUp = VK_NULL_HANDLE;
	VkRenderPass m_rpFinal = VK_NULL_HANDLE;

	// ── Layouts & pipelines ─────────────────────────────────────
	VkDescriptorSetLayout m_globalLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_textureLayout = VK_NULL_HANDLE;
	VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
	struct Pipelines
	{
		VkPipeline skyLut = VK_NULL_HANDLE;
		VkPipeline skyEnv = VK_NULL_HANDLE;
		VkPipeline clouds = VK_NULL_HANDLE;
		VkPipeline shadow = VK_NULL_HANDLE;
		VkPipeline shadowCutout = VK_NULL_HANDLE;
		VkPipeline terrain = VK_NULL_HANDLE;
		VkPipeline terrainCutout = VK_NULL_HANDLE;
		VkPipeline particles = VK_NULL_HANDLE;
		VkPipeline lod = VK_NULL_HANDLE;
		VkPipeline volumetric = VK_NULL_HANDLE;
		VkPipeline fogApply = VK_NULL_HANDLE;
		VkPipeline sky = VK_NULL_HANDLE;
		VkPipeline blit = VK_NULL_HANDLE;
		VkPipeline water = VK_NULL_HANDLE;
		VkPipeline bloomDown = VK_NULL_HANDLE;
		VkPipeline bloomUp = VK_NULL_HANDLE;
		VkPipeline composite = VK_NULL_HANDLE;
	} m_pipe;

	// ── Samplers ────────────────────────────────────────────────
	VkSampler m_samplerBlocks = VK_NULL_HANDLE;
	VkSampler m_samplerLinearClamp = VK_NULL_HANDLE;
	VkSampler m_samplerLinearRepeat = VK_NULL_HANDLE;
	VkSampler m_samplerSky = VK_NULL_HANDLE;
	VkSampler m_samplerNearestClamp = VK_NULL_HANDLE;
	VkSampler m_samplerShadow = VK_NULL_HANDLE;

	// ── Images ──────────────────────────────────────────────────
	VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
	BlockTextures m_blockTex;
	GpuImage m_cloudNoise;
	GpuImage m_skyLut, m_skyEnv;
	GpuImage m_shadowMap;
	GpuImage m_sceneColor, m_linearDepth, m_sceneDepth, m_hdrColor, m_bloom;
	GpuImage m_cloudTex; // nuages à mi-résolution (rgb diffusé, a transmittance)
	GpuImage m_finalDepth;    // profondeur linéaire après l'eau
	GpuImage m_volumetricTex; // brume volumétrique, mi-résolution (rgb, a = T)
	GpuImage m_hdrFog;        // image HDR finale (brume appliquée)
	GpuImage m_panorama, m_logo;
	static constexpr uint32_t BLOOM_MIPS = 5;
	static constexpr uint32_t SKY_LUT_W = 256, SKY_LUT_H = 128;
	static constexpr uint32_t SKY_ENV_W = 512, SKY_ENV_H = 256;

	VkFramebuffer m_fbSkyLut = VK_NULL_HANDLE, m_fbSkyEnv = VK_NULL_HANDLE;
	VkFramebuffer m_fbShadow = VK_NULL_HANDLE;
	VkFramebuffer m_fbScene = VK_NULL_HANDLE, m_fbWater = VK_NULL_HANDLE;
	VkFramebuffer m_fbClouds = VK_NULL_HANDLE;
	VkFramebuffer m_fbVolumetric = VK_NULL_HANDLE, m_fbFogApply = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> m_fbBloom;

	VkExtent2D m_renderExtent{0, 0};
	int m_shadowRes = 0;
	float m_renderScale = 1.0f;
	bool m_vsync = true;

	// ── Descripteurs ────────────────────────────────────────────
	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_globalSets{};
	VkDescriptorSet m_setHdr = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> m_setBloomMips;
	std::array<GpuBuffer, MAX_FRAMES_IN_FLIGHT> m_uniforms{};

	// ── Commandes & synchronisation ─────────────────────────────
	VkCommandPool m_commandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_commandBuffers;
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	// Un sémaphore "render finished" par image du swapchain
	std::vector<VkSemaphore> m_renderFinishedSemaphores;
	std::vector<VkFence> m_inFlightFences;
	uint32_t m_currentFrame = 0;
	uint64_t m_frameNumber = 0;

	// ── Particules : un buffer d'instances par frame en vol ────
	std::array<GpuBuffer, MAX_FRAMES_IN_FLIGHT> m_particleBuffers{};
	std::vector<ParticleInstance> m_pendingParticles;
	uint32_t m_particleCount = 0;

	// ── Cache de la shadow map ──────────────────────────────────
	// La shadow map n'est recalculée que si la caméra s'est déplacée, si le
	// soleil ou les réglages ont changé, ou si des chunks proches ont changé.
	bool m_shadowDirty = true;
	bool m_shadowRefresh = false;  // recalcul pendant la frame courante
	glm::dvec3 m_shadowOrigin{0.0};
	glm::vec3 m_shadowSun{0.0f};
	float m_shadowDistUsed = 0.0f;
	glm::mat4 m_shadowMatrix{1.0f}; // relative à m_shadowOrigin

	// ── Chronométrage GPU ───────────────────────────────────────
	std::array<VkQueryPool, MAX_FRAMES_IN_FLIGHT> m_queryPools{};
	std::array<bool, MAX_FRAMES_IN_FLIGHT> m_queryValid{};
	float m_timestampPeriod = 0.0f; // ns par tick (0 = non supporté)
	void readTimestamps();

	// ── Chunks ──────────────────────────────────────────────────
	struct IVec2Hasher
	{
		size_t operator()(const glm::ivec2 &v) const
		{
			return std::hash<int64_t>()((static_cast<int64_t>(v.x) << 32)
										^ static_cast<uint32_t>(v.y));
		}
	};
	std::unordered_map<glm::ivec2, GpuChunk, IVec2Hasher> m_chunks;
	struct GpuLod
	{
		GpuBuffer buffer;
		VkDeviceSize indexOffset = 0;
		uint32_t indexCount = 0;
		glm::ivec2 tile{0};
		float minY = 0.0f, maxY = 0.0f;
	};
	std::unordered_map<glm::ivec2, GpuLod, IVec2Hasher> m_lods;
	VkDeviceSize m_lodMemory = 0;
	std::vector<std::pair<uint64_t, GpuBuffer>> m_deletionQueue;
	VkDeviceSize m_chunkMemory = 0;
	Stats m_stats;

	// ── ImGui ───────────────────────────────────────────────────
	VkDescriptorPool m_imguiPool = VK_NULL_HANDLE;
	VkDescriptorSet m_panoramaId = VK_NULL_HANDLE;
	VkDescriptorSet m_logoId = VK_NULL_HANDLE;
	std::vector<GpuImage> m_iconImages;
	std::array<VkDescriptorSet, 256> m_blockIcons{};
	void loadBlockIcons();

	// ── Capture d'écran ─────────────────────────────────────────
	std::string m_screenshotPath;
	GpuBuffer m_screenshotBuffer;

	// ── Initialisation ──────────────────────────────────────────
	void createRenderPasses();
	void createLayouts();
	void createPipelines();
	void createSamplers();
	void createStaticResources();
	void createShadowMap(int resolution);
	void destroyShadowMap();
	void createRenderTargets();
	void destroyRenderTargets();
	void createDescriptors();
	void writeDescriptors();
	void createCommandsAndSync();
	void createRenderFinishedSemaphores();
	void destroyRenderFinishedSemaphores();
	void initImGui();
	void recreateSwapchain();
	void applySettings(const RenderSettings &settings);

	// ── Frame ───────────────────────────────────────────────────
	void fillUniforms(FrameUniforms &u, const FrameParams &params,
					  const RenderSettings &settings);
	void recordFrame(VkCommandBuffer cmd, uint32_t imageIndex,
					 const FrameParams &params, const RenderSettings &settings,
					 const FrameUniforms &u, ImDrawData *drawData);
	void beginPass(VkCommandBuffer cmd, VkRenderPass rp, VkFramebuffer fb,
				   VkExtent2D extent, const std::vector<VkClearValue> &clears);
	void drawFullscreen(VkCommandBuffer cmd, VkPipeline pipeline);
	void flushDeletionQueue(bool all);

	void recordScreenshotCopy(VkCommandBuffer cmd, uint32_t imageIndex);
	void saveScreenshot();
};
