#include "atmosphere.h"

#include <cmath>

namespace
	{
		constexpr float R_PLANET = 6371e3f;
		constexpr float R_ATMO = 6471e3f;
		constexpr float H_RAYLEIGH = 8e3f;
		constexpr float H_MIE = 1.2e3f;
		const glm::vec3 BETA_RAYLEIGH(5.5e-6f, 13.0e-6f, 22.4e-6f);
		constexpr float BETA_MIE = 21e-6f;
	}

namespace atmosphere
	{
		glm::vec3 sunDirection(float elevationDeg, float azimuthDeg)
		{
			float e = glm::radians(elevationDeg);
			float a = glm::radians(azimuthDeg);
			return glm::normalize(glm::vec3(std::cos(e) * std::sin(a),
											std::sin(e),
											std::cos(e) * std::cos(a)));
		}

		glm::vec3 sunTransmittance(const glm::vec3 &sunDir)
		{
			glm::vec3 ro(0.0f, R_PLANET + 100.0f, 0.0f);
			glm::vec3 rd = glm::normalize(sunDir);

			// Intersection avec le sommet de l'atmosphère
			float b = glm::dot(ro, rd);
			float c = glm::dot(ro, ro) - R_ATMO * R_ATMO;
			float disc = b * b - c;
			if(disc < 0.0f)
				return glm::vec3(0.0f);
			float tMax = -b + std::sqrt(disc);

			// Soleil sous l'horizon : bloqué par la planète
			float cp = glm::dot(ro, ro) - R_PLANET * R_PLANET;
			float dp = b * b - cp;
			if(dp > 0.0f && -b - std::sqrt(dp) > 0.0f)
				return glm::vec3(0.0f);

			const int steps = 48;
			float dt = tMax / steps;
			float odR = 0.0f, odM = 0.0f;
			for(int i = 0; i < steps; ++i)
				{
					glm::vec3 p = ro + rd * (dt * (i + 0.5f));
					float h = glm::length(p) - R_PLANET;
					odR += std::exp(-h / H_RAYLEIGH) * dt;
					odM += std::exp(-h / H_MIE) * dt;
				}
			glm::vec3 tau = BETA_RAYLEIGH * odR + glm::vec3(BETA_MIE * 1.1f * odM);
			return glm::exp(-tau);
		}
	}
