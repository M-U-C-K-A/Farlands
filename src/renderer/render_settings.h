// -----------------------------------------------------------------------------
// Fichier : render_settings.h
// Rôle : Réglages graphiques modifiables en jeu (menu Options) et préréglages
// d'ambiance ("shaders") : Réaliste, Golden Hour, Horreur.
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>

enum class ShaderPreset : int
{
	Realistic = 0, // journée claire, lac turquoise
	GoldenHour,    // soleil bas, brume dorée, rayons volumétriques
	Horror,        // ciel couvert, brouillard dense, couleurs délavées
	Night,         // clair de lune, étoiles, lanternes et torches
	Custom,        // réglages modifiés à la main
	Count
};

struct RenderSettings
{
	ShaderPreset preset = ShaderPreset::Realistic;

	// Monde
	int renderDistance = 20; // chunks complets (blocs éditables)
	int lodDistance = 2048;  // terrain lointain simplifié (blocs, 0 = aucun)
	bool fallingLeaves = true;
	float flySpeed = 1.0f; // multiplicateur de la vitesse de vol

	// Image
	float renderScale = 1.0f; // résolution interne / résolution écran
	float fov = 70.0f;        // champ de vision vertical (degrés)
	bool vsync = true;
	bool showFps = false;     // compteur de FPS en jeu

	// Soleil
	float sunElevation = 28.0f; // degrés au-dessus de l'horizon
	float sunAzimuth = 215.0f;  // degrés
	bool squareSun = false;     // disque solaire carré (style Minecraft)
	bool night = false;         // nuit : la lune éclaire (azimut opposé au soleil)
	float moonElevation = 34.0f;

	// Ombres
	bool shadows = true;
	int shadowResolution = 4096;
	float shadowDistance = 112.0f; // demi-largeur de la zone ombrée (blocs)

	// Atmosphère
	bool clouds = true;
	float cloudCoverage = 0.58f;
	float cloudDensity = 1.0f;    // multiplicateur d'épaisseur optique
	float cloudBrightness = 1.0f; // < 1 : nuages d'orage
	float fogDensity = 1.0f;      // brume atmosphérique (perspective aérienne)

	// Brouillard volumétrique (brume au sol + rayons de lumière)
	bool volumetricFog = true;
	float volumetricDensity = 0.35f; // densité de la brume
	float mistHeight = 14.0f;        // épaisseur de la brume au-dessus de l'eau
	float lightShafts = 1.0f;        // intensité des rayons (diffusion avant)

	// Eau
	bool waterReflections = true; // réflexions écran (SSR)

	// Post-traitement / étalonnage
	bool bloom = true;
	float bloomStrength = 0.05f;
	bool godRays = true;
	float exposure = 1.0f;
	float saturation = 1.08f;
	float contrast = 1.0f;
	glm::vec3 colorFilter{1.0f, 1.0f, 1.0f}; // teinte multiplicative
	glm::vec3 shadowTint{1.0f, 1.0f, 1.0f};  // teinte des tons sombres
	float vignette = 0.35f;
};

/// Nom affichable d'un préréglage
inline const char *presetName(ShaderPreset p)
{
	switch(p)
		{
		case ShaderPreset::Realistic: return "Realistic";
		case ShaderPreset::GoldenHour: return "Golden Hour";
		case ShaderPreset::Horror: return "Horror";
		case ShaderPreset::Night: return "Night";
		default: return "Custom";
		}
}

/// Applique l'ambiance d'un préréglage (conserve les réglages de
/// performance : distances, résolution, VSync, ombres...)
inline void applyPreset(RenderSettings &s, ShaderPreset p)
{
	RenderSettings d; // valeurs par défaut = Réaliste
	s.preset = p;
	s.sunElevation = d.sunElevation;
	s.squareSun = false;
	s.night = false;
	s.moonElevation = d.moonElevation;
	s.clouds = true;
	s.cloudCoverage = d.cloudCoverage;
	s.cloudDensity = d.cloudDensity;
	s.cloudBrightness = d.cloudBrightness;
	s.fogDensity = d.fogDensity;
	s.volumetricFog = true;
	s.volumetricDensity = d.volumetricDensity;
	s.mistHeight = d.mistHeight;
	s.lightShafts = d.lightShafts;
	s.bloomStrength = d.bloomStrength;
	s.godRays = true;
	s.exposure = d.exposure;
	s.saturation = d.saturation;
	s.contrast = d.contrast;
	s.colorFilter = d.colorFilter;
	s.shadowTint = d.shadowTint;
	s.vignette = d.vignette;
	s.fallingLeaves = true;

	switch(p)
		{
		case ShaderPreset::GoldenHour:
			s.sunElevation = 6.5f;
			s.cloudCoverage = 0.62f;
			s.cloudDensity = 1.3f;
			s.fogDensity = 1.6f;
			s.volumetricDensity = 1.0f;
			s.mistHeight = 22.0f;
			s.lightShafts = 2.2f;
			s.bloomStrength = 0.09f;
			s.exposure = 1.15f;
			s.saturation = 1.18f;
			s.contrast = 1.08f;
			s.colorFilter = glm::vec3(1.08f, 0.98f, 0.82f);
			s.shadowTint = glm::vec3(0.85f, 0.95f, 1.12f);
			s.vignette = 0.45f;
			break;
		case ShaderPreset::Horror:
			s.sunElevation = 24.0f;
			s.squareSun = true;
			s.cloudCoverage = 0.86f;
			s.cloudDensity = 1.8f;
			s.cloudBrightness = 0.42f;
			s.fogDensity = 3.0f;
			s.volumetricDensity = 2.0f;
			s.mistHeight = 34.0f;
			s.lightShafts = 0.8f;
			s.bloomStrength = 0.10f;
			s.exposure = 0.95f;
			s.saturation = 0.3f;
			s.contrast = 1.2f;
			s.colorFilter = glm::vec3(0.96f, 0.94f, 0.86f);
			s.shadowTint = glm::vec3(0.80f, 0.88f, 0.78f);
			s.vignette = 0.8f;
			break;
		case ShaderPreset::Night:
			s.night = true;
			s.cloudCoverage = 0.46f;
			s.fogDensity = 1.3f;
			s.volumetricDensity = 0.7f;
			s.mistHeight = 18.0f;
			s.lightShafts = 1.5f;
			s.bloomStrength = 0.11f;
			s.exposure = 2.3f;
			s.saturation = 0.85f;
			s.contrast = 1.06f;
			s.colorFilter = glm::vec3(0.90f, 0.96f, 1.10f);
			s.shadowTint = glm::vec3(0.82f, 0.90f, 1.18f);
			s.vignette = 0.55f;
			break;
		default: break;
		}
}
