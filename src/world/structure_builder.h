// -----------------------------------------------------------------------------
// Fichier : structure_builder.h
// Rôle : Outils internes de construction des structures : générateur
// aléatoire déterministe, pose de blocs en coordonnées locales tournées
// (porte locale vers -Z), blocs orientés (escaliers, portes) et rectangles.
// -----------------------------------------------------------------------------
#pragma once

#include "structures.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace structgen
	{
		// Générateur pseudo-aléatoire local (xorshift), déterministe par structure
		struct Rng
		{
			uint32_t s;
			explicit Rng(uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
			uint32_t next()
			{
				s ^= s << 13;
				s ^= s >> 17;
				s ^= s << 5;
				return s;
			}
			float f() { return static_cast<float>(next() >> 8) / 16777216.0f; }
			int range(int a, int b) { return a + static_cast<int>(next() % static_cast<uint32_t>(b - a + 1)); }
			bool chance(float p) { return f() < p; }
		};

		// Décalage tourné (rot : 0..3, porte locale vers -Z ; 1 : +X, 2 : +Z, 3 : -X)
		inline glm::ivec2 rotate2(int x, int z, int rot)
		{
			switch(rot & 3)
				{
				case 0: return {x, z};
				case 1: return {-z, x};
				case 2: return {-x, -z};
				default: return {z, -x};
				}
		}

		// Pose de blocs en coordonnées locales tournées
		struct Builder
		{
			using Mode = StructureBlueprint::Mode;
			StructureBlueprint &bp;
			glm::ivec3 origin{0};
			int rot = 0;

			glm::ivec3 w(int x, int y, int z) const
			{
				glm::ivec2 r = rotate2(x, z, rot);
				return origin + glm::ivec3(r.x, y, r.y);
			}
			void set(int x, int y, int z, BlockType t, Mode m = Mode::Set) { bp.add(w(x, y, z), t, m); }
			void clear(int x, int y, int z) { bp.add(w(x, y, z), BlockType::Air, Mode::Clear); }
			/// Bloc orienté (escaliers, portes) : orientation locale -> monde
			void face(int x, int y, int z, BlockType base, int localFacing, Mode m = Mode::Set)
			{
				bp.add(w(x, y, z), oriented(base, localFacing + rot), m);
			}
			/// Porte de deux blocs, panneau du côté localFacing
			void door(int x, int y, int z, int localFacing)
			{
				face(x, y, z, BlockType::Oak_Door_Lower, localFacing);
				face(x, y + 1, z, BlockType::Oak_Door_Upper, localFacing);
			}
			void box(int x0, int y0, int z0, int x1, int y1, int z1, BlockType t, Mode m = Mode::Set)
			{
				for(int y = y0; y <= y1; ++y)
					for(int z = z0; z <= z1; ++z)
						for(int x = x0; x <= x1; ++x)
							set(x, y, z, t, m);
			}
		};

		struct Rect
		{
			int x0, z0, x1, z1;
			bool overlaps(const Rect &o, int pad) const
			{
				return x0 - pad <= o.x1 && x1 + pad >= o.x0 && z0 - pad <= o.z1 && z1 + pad >= o.z0;
			}
			bool contains(int x, int z) const { return x >= x0 && x <= x1 && z >= z0 && z <= z1; }
		};
	}
