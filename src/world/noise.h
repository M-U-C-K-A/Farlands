// -----------------------------------------------------------------------------
// Fichier : noise.h
// Rôle : Implémentation du bruit de Perlin avec support des octaves.
// Utilisé pour la génération procédurale du terrain.
// -----------------------------------------------------------------------------
#pragma once

#include <cstdint>

class PerlinNoise
{
  public:
	/// Constructeur avec graine (seed) pour la reproductibilité
	explicit PerlinNoise(uint32_t seed = 42);

	/// Bruit Perlin 2D brut, sortie dans [-1, 1]
	float noise(float x, float y) const;

	/// Bruit avec octaves (fBm — Fractional Brownian Motion)
	/// @param x, y     Coordonnées monde
	/// @param octaves  Nombre de couches (plus = plus de détail)
	/// @param lacunarity Multiplicateur de fréquence par octave (typiquement 2.0)
	/// @param persistence Multiplicateur d'amplitude par octave (typiquement 0.5)
	/// @return Valeur dans [-1, 1] approximativement
	float octaveNoise(float x, float y, int octaves = 6,
					  float lacunarity = 2.0f,
					  float persistence = 0.5f) const;

  private:
	uint8_t m_perm[512]; // Table de permutation doublée

	float fade(float t) const;
	float lerp(float a, float b, float t) const;
	float grad(int hash, float x, float y) const;
};
