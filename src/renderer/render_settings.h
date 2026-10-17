// -----------------------------------------------------------------------------
// Fichier : render_settings.h
// Rôle : Réglages graphiques modifiables en jeu (menu Options).
// -----------------------------------------------------------------------------
#pragma once

struct RenderSettings
{
	// Monde
	int renderDistance = 20; // en chunks

	// Image
	float renderScale = 1.0f; // résolution interne / résolution écran
	float fov = 70.0f;        // champ de vision vertical (degrés)
	bool vsync = true;

	// Soleil
	float sunElevation = 28.0f; // degrés au-dessus de l'horizon
	float sunAzimuth = 215.0f;  // degrés

	// Ombres
	bool shadows = true;
	int shadowResolution = 4096;
	float shadowDistance = 112.0f; // demi-largeur de la zone ombrée (blocs)

	// Atmosphère
	bool clouds = true;
	float cloudCoverage = 0.58f;
	float fogDensity = 1.0f; // multiplicateur de la brume atmosphérique

	// Eau
	bool waterReflections = true; // réflexions écran (SSR)

	// Post-traitement
	bool bloom = true;
	float bloomStrength = 0.05f;
	bool godRays = true;
	float exposure = 1.0f;
	float saturation = 1.08f;
};
