// -----------------------------------------------------------------------------
// Fichier : block_shape.h
// Rôle : Géométrie des blocs non cubiques (escaliers, dalles, vitres,
// barrières, portes) sous forme de boîtes alignées, partagée par le maillage
// (rendu) et le joueur (collisions).
// -----------------------------------------------------------------------------
#pragma once

#include "block.h"

#include <glm/glm.hpp>

struct ShapeBox
{
	glm::vec3 lo, hi; // coordonnées locales au bloc [0, 1]
};

namespace shape
	{
		/// Une vitre se relie aux vitres, au verre et aux cubes opaques
		inline bool paneConnects(BlockType t)
		{
			const BlockData &d = BlockDatabase::Get(t);
			return d.renderType == RenderType::Pane || t == BlockType::Glass
				   || BlockDatabase::IsOpaqueCube(t);
		}
		/// Une barrière se relie aux barrières et aux cubes opaques
		inline bool fenceConnects(BlockType t)
		{
			return BlockDatabase::Get(t).renderType == RenderType::Fence
				   || BlockDatabase::IsOpaqueCube(t);
		}

		/// Boîtes d'un bloc ; neighbor(dx, dz) donne le voisin horizontal.
		/// collision : barrières hautes d'un bloc et demi, portes traversables.
		template <class Neighbor>
		int boxes(BlockType t, Neighbor neighbor, bool collision, ShapeBox out[6])
		{
			const BlockData &d = BlockDatabase::Get(t);
			const float p = 1.0f / 16.0f;
			switch(d.renderType)
				{
				case RenderType::Slab: out[0] = {{0, 0, 0}, {1, 0.5f, 1}}; return 1;
				case RenderType::Stairs:
					{
						out[0] = {{0, 0, 0}, {1, 0.5f, 1}};
						glm::vec3 lo(0, 0.5f, 0), hi(1, 1, 1);
						switch(d.facing)
							{
							case 0: hi.z = 0.5f; break;
							case 1: lo.x = 0.5f; break;
							case 2: lo.z = 0.5f; break;
							default: hi.x = 0.5f; break;
							}
						out[1] = {lo, hi};
						return 2;
					}
				case RenderType::Door:
					{
						if(collision)
							return 0;
						const float t3 = 3.0f * p;
						glm::vec3 lo(0), hi(1);
						switch(d.facing)
							{
							case 0: hi.z = t3; break;
							case 1: lo.x = 1.0f - t3; break;
							case 2: lo.z = 1.0f - t3; break;
							default: hi.x = t3; break;
							}
						out[0] = {lo, hi};
						return 1;
					}
				case RenderType::Pane:
				case RenderType::Fence:
					{
						bool fence = d.renderType == RenderType::Fence;
						auto connects = [&](int dx, int dz) {
							BlockType nb = neighbor(dx, dz);
							return fence ? fenceConnects(nb) : paneConnects(nb);
						};
						bool n = connects(0, -1), s = connects(0, 1), e = connects(1, 0), w = connects(-1, 0);
						float r = fence ? 2.0f * p : 1.0f * p; // demi-épaisseur
						float top = fence && collision ? 1.5f : 1.0f;
						int count = 0;
						if(fence || (!n && !s && !e && !w))
							{
								// Poteau (barrière) ; vitre isolée : plaque est-ouest
								if(fence)
									out[count++] = {{0.5f - r, 0, 0.5f - r}, {0.5f + r, top, 0.5f + r}};
								else
									out[count++] = {{0, 0, 0.5f - r}, {1, 1, 0.5f + r}};
							}
						auto arm = [&](glm::vec3 lo, glm::vec3 hi) {
							if(!fence || collision)
								{
									if(fence)
										{
											lo.y = 0.0f;
											hi.y = top;
										}
									out[count++] = {lo, hi};
									return;
								}
							// Barrière : deux traverses
							out[count++] = {{lo.x, 6 * p, lo.z}, {hi.x, 9 * p, hi.z}};
							out[count++] = {{lo.x, 12 * p, lo.z}, {hi.x, 15 * p, hi.z}};
						};
						float a = fence ? 1.0f * p : r; // demi-épaisseur des traverses
						if(e || w)
							arm({w ? 0.0f : 0.5f - a, 0, 0.5f - a}, {e ? 1.0f : 0.5f + a, 1, 0.5f + a});
						if(n || s)
							arm({0.5f - a, 0, n ? 0.0f : 0.5f - a}, {0.5f + a, 1, s ? 1.0f : 0.5f + a});
						return count;
					}
				default: out[0] = {{0, 0, 0}, {1, 1, 1}}; return 1;
				}
		}
	}
