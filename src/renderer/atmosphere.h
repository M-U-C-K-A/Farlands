// -----------------------------------------------------------------------------
// Fichier : atmosphere.h
// Rôle : Calculs atmosphériques côté CPU (une fois par frame) : direction du
// soleil et couleur de la lumière solaire après traversée de l'atmosphère
// (diffusion de Rayleigh et de Mie). Constantes identiques à
// shaders/atmosphere.glsl.
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>

namespace atmosphere
	{
		/// Direction (normalisée) vers le soleil
		glm::vec3 sunDirection(float elevationDeg, float azimuthDeg);

		/// Transmittance de l'atmosphère du sol vers le soleil (0..1 par canal)
		glm::vec3 sunTransmittance(const glm::vec3 &sunDir);
	}
