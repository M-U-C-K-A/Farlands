// -----------------------------------------------------------------------------
// Fichier : noise.cpp
// Rôle : Implémentation du bruit de Perlin 2D avec octaves.
// Basé sur l'algorithme classique de Ken Perlin (improved noise, 2002).
// -----------------------------------------------------------------------------
#include "noise.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

// Constructeur : génère une table de permutation à partir de la seed
PerlinNoise::PerlinNoise(uint32_t seed)
{
	// Initialiser [0..255]
	uint8_t p[256];
	for(int i = 0; i < 256; ++i)
		p[i] = static_cast<uint8_t>(i);

	// Mélanger avec la seed
	std::mt19937 rng(seed);
	for(int i = 255; i > 0; --i)
		{
			std::uniform_int_distribution<int> dist(0, i);
			std::swap(p[i], p[dist(rng)]);
		}

	// Doubler la table (pour éviter les modulos)
	for(int i = 0; i < 512; ++i)
		m_perm[i] = p[i & 255];
}

// Courbe de lissage quintic (6t⁵ - 15t⁴ + 10t³)
float PerlinNoise::fade(float t) const
{
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float PerlinNoise::lerp(float a, float b, float t) const
{
	return a + t * (b - a);
}

// Gradient pseudo-aléatoire 2D
float PerlinNoise::grad(int hash, float x, float y) const
{
	// 4 directions de gradient : (1,1), (-1,1), (1,-1), (-1,-1)
	// + 4 axes : (1,0), (-1,0), (0,1), (0,-1)
	int h = hash & 7;
	float u = h < 4 ? x : y;
	float v = h < 4 ? y : x;
	return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

// Bruit Perlin 2D brut
float PerlinNoise::noise(float x, float y) const
{
	// Coordonnées de la cellule
	int xi = static_cast<int>(std::floor(x)) & 255;
	int yi = static_cast<int>(std::floor(y)) & 255;

	// Position relative dans la cellule [0, 1]
	float xf = x - std::floor(x);
	float yf = y - std::floor(y);

	// Courbes de lissage
	float u = fade(xf);
	float v = fade(yf);

	// Hash des 4 coins
	int a = m_perm[xi] + yi;
	int b = m_perm[xi + 1] + yi;

	// Interpolation bilinéaire des gradients
	float x1 = lerp(grad(m_perm[a], xf, yf),
					 grad(m_perm[b], xf - 1.0f, yf), u);
	float x2 = lerp(grad(m_perm[a + 1], xf, yf - 1.0f),
					 grad(m_perm[b + 1], xf - 1.0f, yf - 1.0f), u);

	return lerp(x1, x2, v);
}

// Bruit avec octaves (fBm)
float PerlinNoise::octaveNoise(float x, float y, int octaves,
							   float lacunarity, float persistence) const
{
	float total = 0.0f;
	float amplitude = 1.0f;
	float frequency = 1.0f;
	float maxValue = 0.0f; // Pour normaliser

	for(int i = 0; i < octaves; ++i)
		{
			total += noise(x * frequency, y * frequency) * amplitude;
			maxValue += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}

	return total / maxValue; // Normalise dans [-1, 1]
}
