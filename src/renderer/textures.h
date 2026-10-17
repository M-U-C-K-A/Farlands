// -----------------------------------------------------------------------------
// Fichier : textures.h
// Rôle : Chargement / génération des textures GPU :
//  - tableau de textures des blocs (albédo sRGB + normal maps déduites de la
//    luminance), avec mipmaps ;
//  - textures 2D de l'interface (panorama, logo) ;
//  - bruit 3D périodique (Perlin-Worley) pour les nuages volumétriques.
// -----------------------------------------------------------------------------
#pragma once

#include "vk_utils.h"

#include <string>
#include <vector>

struct BlockTextures
{
	GpuImage albedo;
	GpuImage normal;
};

namespace textures
	{
		/// Charge les textures de blocs (chemins absolus, "" = couche vide)
		BlockTextures loadBlockTextures(VkContext &ctx,
										const std::vector<std::string> &paths);

		/// Texture 2D sRGB sans mipmaps (interface)
		GpuImage loadTexture2D(VkContext &ctx, const std::string &path);

		/// Bruit 3D périodique RGBA8 (64³, mipmappé) pour les nuages
		GpuImage createCloudNoise(VkContext &ctx);
	}
