// -----------------------------------------------------------------------------
// Fichier : village.cpp
// Rôle : Génération des grands villages.
//
// Étapes : place centrale (fontaine ou puits, étals) -> rues principales qui
// serpentent depuis la place, ruelles qui s'en détachent -> emplacements le
// long des rues : d'abord les bâtiments remarquables (église, forge,
// taverne, bibliothèque) près de la place, puis les maisons -> remparts
// (tours, portes) autour du bâti pour les grands villages -> champs, moulin,
// grange et enclos le long des chemins extérieurs -> port si une rue
// principale descend jusqu'à l'eau -> lampadaires, masque sans arbres.
// Tout est décidé sur une grille d'occupation, puis écrit dans le plan.
// -----------------------------------------------------------------------------
#include "village.h"

#include "structure_builder.h"
#include "terrain_generator.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace structgen;
using Mode = StructureBlueprint::Mode;

namespace
	{
		constexpr float PI = 3.14159265f;

		enum Cell : uint8_t
		{
			Free = 0,
			RoadCell,
			PlazaCell,
			BuildingCell,
			FieldCell,
			WallCell,
			DockCell,
		};

		// ── Hauteurs du terrain, calculées à la demande ─────────────
		struct Heights
		{
			const TerrainGenerator &gen;
			int x0, z0, size;
			std::vector<int16_t> v;
			Heights(const TerrainGenerator &g, int cx, int cz, int r)
				: gen(g), x0(cx - r), z0(cz - r), size(2 * r + 1), v(static_cast<size_t>(size) * size, -1)
			{}
			int operator()(int x, int z)
			{
				int i = x - x0, k = z - z0;
				if(i < 0 || k < 0 || i >= size || k >= size)
					return gen.getHeightAt(x, z);
				int16_t &h = v[static_cast<size_t>(k) * size + i];
				if(h < 0)
					h = static_cast<int16_t>(gen.getHeightAt(x, z));
				return h;
			}
		};

		// ── Grille d'occupation ─────────────────────────────────────
		struct Grid
		{
			int x0, z0, size;
			std::vector<uint8_t> c;
			Grid(int cx, int cz, int r) : x0(cx - r), z0(cz - r), size(2 * r + 1), c(static_cast<size_t>(size) * size, Free) {}
			bool inside(int x, int z, int margin = 0) const
			{
				return x - x0 >= margin && z - z0 >= margin && x - x0 < size - margin && z - z0 < size - margin;
			}
			uint8_t get(int x, int z) const
			{
				return inside(x, z) ? c[static_cast<size_t>(z - z0) * size + (x - x0)] : 0xFF;
			}
			void set(int x, int z, uint8_t v)
			{
				if(inside(x, z))
					c[static_cast<size_t>(z - z0) * size + (x - x0)] = v;
			}
			bool free(const Rect &r) const
			{
				for(int z = r.z0; z <= r.z1; ++z)
					for(int x = r.x0; x <= r.x1; ++x)
						if(get(x, z) != Free)
							return false;
				return true;
			}
			void fill(const Rect &r, uint8_t v)
			{
				for(int z = r.z0; z <= r.z1; ++z)
					for(int x = r.x0; x <= r.x1; ++x)
						set(x, z, v);
			}
		};

		// ── Matériaux d'un style ────────────────────────────────────
		struct Palette
		{
			BlockType wall, lower, frame, floor, foundation;
			BlockType roof, roofFull; // escaliers (base orientée) et faîtage
			BlockType fence, plaza, plaza2, wallStone;
			bool timber = true; // poteaux de colombage
			bool flatRoofs = false;
		};

		Palette basePalette(int style)
		{
			switch(style)
				{
				case 1: // taïga
					return {BlockType::Spruce_Planks, BlockType::Cobblestone,     BlockType::Spruce_Log,
							BlockType::Spruce_Planks, BlockType::Cobblestone,     BlockType::Slate_Stairs,
							BlockType::Slate,         BlockType::Spruce_Fence,    BlockType::Cobblestone,
							BlockType::Gravel,        BlockType::Mossy_Cobblestone, false};
				case 2: // cerisiers
					return {BlockType::White_Plaster, BlockType::Stone_Bricks,     BlockType::Vermilion_Planks,
							BlockType::Cherry_Planks, BlockType::Stone_Bricks,     BlockType::Slate_Stairs,
							BlockType::Slate,         BlockType::Spruce_Fence,     BlockType::Stone_Bricks,
							BlockType::Gravel,        BlockType::Stone_Bricks};
				case 3: // neige
					return {BlockType::Spruce_Planks, BlockType::Cobblestone, BlockType::Spruce_Log,
							BlockType::Plank,         BlockType::Cobblestone, BlockType::Spruce_Stairs,
							BlockType::Spruce_Planks, BlockType::Spruce_Fence, BlockType::Cobblestone,
							BlockType::Gravel,        BlockType::Stone_Bricks, false};
				case 4: // désert
					return {BlockType::Sandstone,       BlockType::Sandstone,  BlockType::Chiseled_Sandstone,
							BlockType::Sandstone,       BlockType::Sandstone,  BlockType::Sandstone_Stairs,
							BlockType::Sandstone,       BlockType::Oak_Fence,  BlockType::Sandstone,
							BlockType::Chiseled_Sandstone, BlockType::Sandstone, false, true};
				default: // plaines : colombages, tuiles
					return {BlockType::White_Plaster, BlockType::Cobblestone, BlockType::Log,
							BlockType::Plank,         BlockType::Cobblestone, BlockType::Roof_Tile_Stairs,
							BlockType::Roof_Tiles,    BlockType::Oak_Fence,   BlockType::Cobblestone,
							BlockType::Stone_Bricks,  BlockType::Stone_Bricks};
				}
		}

		struct Road
		{
			std::vector<glm::ivec2> pts;
			int halfWidth = 1;
			bool main = false;
			bool water = false;   // s'arrête au bord de l'eau
			glm::vec2 waterDir{0.0f};
		};

		struct Slot
		{
			glm::vec2 p, t; // point de la rue, tangente
			int side;       // côté de la rue (+1 / -1)
			int halfWidth;
			float dist;     // distance au centre
		};

		enum class BType
		{
			HouseSmall,
			HouseMedium,
			HouseLarge,
			Church,
			Smithy,
			Tavern,
			Library,
			Barn,
			Windmill,
			Warehouse,
		};

		struct Placed
		{
			BType type;
			glm::ivec3 origin;
			int rot, w, d;
			Rect rect;
		};

		const BlockType CROPS[4] = {BlockType::Wheat, BlockType::Carrots, BlockType::Potatoes,
									BlockType::Beetroots};
		const BlockType STAINED[3] = {BlockType::Red_Stained_Glass_Pane, BlockType::Blue_Stained_Glass_Pane,
									  BlockType::Yellow_Stained_Glass_Pane};
		const BlockType WOOL[5] = {BlockType::White_Wool, BlockType::Red_Wool, BlockType::Yellow_Wool,
								   BlockType::Blue_Wool, BlockType::Green_Wool};

		// ═════════════════════════════════════════════════════════════
		struct Village
		{
			const TerrainGenerator &gen;
			StructureBlueprint &bp;
			Rng r;
			int style, cx, cz, R;
			Heights H;
			Grid g;
			Palette pal;
			std::vector<Road> roads;
			std::vector<Slot> slots;
			std::vector<Placed> placed;
			int plazaR = 7, plazaY = 0;
			int wallRadius = 0;

			Village(const TerrainGenerator &gn, StructureBlueprint &b, const StructureSite &s)
				: gen(gn), bp(b), r(s.seed), style(s.style), cx(s.center.x), cz(s.center.z), R(s.radius),
				  H(gn, s.center.x, s.center.z, s.radius + 24), g(s.center.x, s.center.z, s.radius + 24),
				  pal(basePalette(s.style))
			{}

			// Sol local (dessus de la colonne) relatif au plancher du bâtiment
			int groundTop(const Builder &b, int x, int z)
			{
				glm::ivec3 p = b.w(x, 0, z);
				return H(p.x, p.z) - 1 - b.origin.y;
			}

			// Fondations jusqu'au sol, vide au-dessus du plancher jusqu'à clearTop
			void groundwork(Builder &b, int x0, int z0, int x1, int z1, BlockType mat, int clearTop)
			{
				for(int z = z0; z <= z1; ++z)
					for(int x = x0; x <= x1; ++x)
						{
							int top = groundTop(b, x, z);
							for(int y = std::min(top, -1); y < 0; ++y)
								b.set(x, y, z, mat);
							bool ring = x == x0 || x == x1 || z == z0 || z == z1;
							if(ring && top < 0)
								b.set(x, 0, z, mat); // socle autour du bâtiment
							for(int y = 1; y <= std::max(clearTop, top + 1); ++y)
								b.clear(x, y, z);
						}
			}

			// Toit à deux pans en escaliers ; faîtage le long de z (ou de x)
			void gableRoof(Builder &b, int w, int d, int y0, BlockType stairs, BlockType full, BlockType gable,
						   bool alongX, int z0 = 0, int over = 1)
			{
				int span = alongX ? d : w, len = alongX ? w : d;
				for(int k = 0;; ++k)
					{
						int lo = -over + k, hi = span - 1 + over - k;
						if(lo > hi)
							break;
						int y = y0 + k;
						for(int t = -over; t <= len - 1 + over; ++t)
							{
								auto put = [&](int a, bool stair, int facing) {
									int x = alongX ? t : a, z = (alongX ? a : t) + z0;
									if(stair)
										b.face(x, y, z, stairs, facing);
									else
										b.set(x, y, z, full);
								};
								if(lo == hi)
									put(lo, false, 0);
								else
									{
										put(lo, true, alongX ? 2 : 1);
										put(hi, true, alongX ? 0 : 3);
									}
							}
						for(int a = std::max(lo + 1, 0); a < std::min(hi, span); ++a)
							for(int t : {0, len - 1})
								b.set(alongX ? t : a, y, (alongX ? a : t) + z0, gable);
					}
			}

			// Toit plat avec acrotère (désert)
			void flatRoof(Builder &b, int w, int d, int y)
			{
				for(int x = -1; x <= w; ++x)
					for(int z = -1; z <= d; ++z)
						b.set(x, y, z, pal.wall);
				for(int x = 0; x < w; ++x)
					for(int z = 0; z < d; ++z)
						if(x == 0 || x == w - 1 || z == 0 || z == d - 1)
							b.set(x, y + 1, z, (x + z) % 2 ? pal.frame : BlockType::Sandstone_Slab);
			}

			// ── Maison paramétrée (1 ou 2 étages) ──────────────────
			void house(const Placed &pl, int floors, bool chimney, bool tavern = false)
			{
				Builder b{bp, pl.origin, pl.rot};
				const int w = pl.w, d = pl.d;
				BlockType roof = pal.roof, roofFull = pal.roofFull;
				if(style == 0 && r.chance(0.3f))
					{
						roof = BlockType::Spruce_Stairs;
						roofFull = BlockType::Spruce_Planks;
					}
				const int top = floors == 2 ? 7 : 3;
				const bool alongX = w > d;
				const int roofHeight = (alongX ? d : w) / 2 + 2;
				groundwork(b, -1, -1, w, d, pal.foundation, top + roofHeight + 2);
				for(int x = 0; x < w; ++x)
					for(int z = 0; z < d; ++z)
						{
							bool edge = x == 0 || x == w - 1 || z == 0 || z == d - 1;
							b.set(x, 0, z, edge ? pal.foundation : pal.floor);
						}
				for(int y = 1; y <= top; ++y)
					for(int x = 0; x < w; ++x)
						for(int z = 0; z < d; ++z)
							{
								bool ex = x == 0 || x == w - 1, ez = z == 0 || z == d - 1;
								if(!ex && !ez)
									continue;
								BlockType m = (floors == 2 && y <= 3) ? pal.lower : pal.wall;
								if(ex && ez)
									m = pal.frame;
								else if(floors == 2 && y == 4)
									m = pal.frame; // poutre entre les étages
								else if(pal.timber && floors == 1 && ((ex && z % 4 == 0) || (ez && x % 4 == 0)))
									m = pal.frame;
								b.set(x, y, z, m);
							}
				// Fenêtres (vitres) à chaque étage
				for(int f = 0; f < floors; ++f)
					{
						int y = 2 + 4 * f;
						for(int z = 1; z < d - 1; ++z)
							if(z % 4 == 2)
								{
									b.set(0, y, z, BlockType::Glass_Pane);
									b.set(w - 1, y, z, BlockType::Glass_Pane);
								}
						for(int x = 1; x < w - 1; ++x)
							if(x % 4 == 2 || (w <= 5 && x == 2))
								{
									if(f > 0 || std::abs(x - w / 2) > 0)
										b.set(x, y, 0, BlockType::Glass_Pane);
									b.set(x, y, d - 1, BlockType::Glass_Pane);
								}
					}
				// Porte et marche
				b.door(w / 2, 1, 0, 0);
				b.set(w / 2, 0, -1, pal.foundation);
				if(floors == 2)
					for(int x = 1; x < w - 1; ++x)
						for(int z = 1; z < d - 1; ++z)
							b.set(x, 4, z, pal.floor);
				// Toit
				if(pal.flatRoofs && !tavern)
					flatRoof(b, w, d, top + 1);
				else
					{
						gableRoof(b, w, d, top + 1, roof, roofFull, pal.wall, alongX);
						// Lucarne dans le pignon des grandes maisons
						if(!alongX && w >= 7)
							b.set(w / 2, top + 2, 0, BlockType::Glass_Pane);
					}
				// Intérieur
				b.set(w / 2, 3, d / 2, BlockType::Lantern);
				if(floors == 2)
					b.set(w / 2, 7, d / 2, BlockType::Lantern);
				if(!tavern)
					{
						b.set(1, 1, d - 2, BlockType::Red_Wool); // lit
						b.set(1, 1, d - 3, BlockType::White_Wool);
						b.set(w - 2, 1, d - 3, BlockType::Crafting_Table);
						b.set(w - 2, 1, 1, BlockType::Barrel);
						if(w >= 7)
							b.set(1, 1, 1, BlockType::Bookshelf);
					}
				if(chimney && !pal.flatRoofs)
					for(int y = 1; y <= top + roofHeight + 1; ++y)
						b.set(w - 2, y, d - 2, BlockType::Bricks);
			}

			// ── Église : nef, clocher avec flèche, vitraux, bancs ──
			void church(const Placed &pl)
			{
				Builder b{bp, pl.origin, pl.rot};
				const int W = 9, T = 5, D = 22; // largeur, profondeur du clocher, totale
				BlockType stone = style == 4 ? BlockType::Sandstone : BlockType::Stone_Bricks;
				BlockType base = style == 4 ? BlockType::Chiseled_Sandstone : BlockType::Cobblestone;
				BlockType roof = style == 4 ? BlockType::Sandstone_Stairs : BlockType::Slate_Stairs;
				BlockType roofFull = style == 4 ? BlockType::Sandstone : BlockType::Slate;
				groundwork(b, -1, -1, W, D, base, 26);
				auto stoneAt = [&](int y) {
					if(y == 1)
						return base;
					if(style != 4 && r.chance(0.12f))
						return BlockType::Mossy_Stone_Bricks;
					return stone;
				};
				// Nef
				for(int x = 0; x < W; ++x)
					for(int z = T - 1; z < D; ++z)
						b.set(x, 0, z, (x == 4 && z > T && z < D - 4) ? BlockType::Red_Wool : stone);
				for(int y = 1; y <= 7; ++y)
					for(int z = T - 1; z < D; ++z)
						for(int x = 0; x < W; ++x)
							{
								bool edge = x == 0 || x == W - 1 || z == T - 1 || z == D - 1;
								if(edge)
									b.set(x, y, z, stoneAt(y));
							}
				// Contreforts
				for(int z = T + 2; z < D - 1; z += 4)
					for(int x : {-1, W})
						{
							for(int y = 1; y <= 5; ++y)
								b.set(x, y, z, stone);
							b.face(x, 6, z, style == 4 ? BlockType::Sandstone_Stairs : BlockType::Stone_Brick_Stairs,
								   x < 0 ? 1 : 3);
						}
				// Vitraux (3 de haut) et rosace
				int c = 0;
				for(int z = T + 4; z < D - 2; z += 4, ++c)
					for(int y = 3; y <= 5; ++y)
						{
							b.set(0, y, z, STAINED[c % 3]);
							b.set(W - 1, y, z, STAINED[(c + 1) % 3]);
						}
				for(int x = 3; x <= 5; ++x)
					for(int y = 4; y <= 6; ++y)
						b.set(x, y, D - 1, (x == 4 && y == 5) ? BlockType::Yellow_Stained_Glass_Pane
															   : BlockType::Blue_Stained_Glass_Pane);
				gableRoof(b, W, D - T + 1, 8, roof, roofFull, stone, false, T - 1);
				// Clocher
				const int towerTop = 19;
				for(int y = 0; y <= towerTop; ++y)
					for(int x = 2; x <= 6; ++x)
						for(int z = 0; z < T; ++z)
							{
								bool edge = x == 2 || x == 6 || z == 0 || z == T - 1;
								if(y == 0 || y == 13 || y == towerTop)
									b.set(x, y, z, stone);
								else if(edge)
									b.set(x, y, z, stoneAt(y));
								else
									b.clear(x, y, z);
							}
				// Beffroi : ouvertures sur les quatre faces, cloche
				for(int y = 15; y <= 17; ++y)
					{
						b.clear(4, y, 0);
						b.clear(4, y, T - 1);
						b.clear(2, y, 2);
						b.clear(6, y, 2);
					}
				b.set(4, 16, 2, BlockType::Gold_Block);
				b.set(4, 17, 2, BlockType::Oak_Fence);
				b.set(4, 18, 2, BlockType::Oak_Fence);
				// Flèche en ardoise
				for(int k = 0; k < 3; ++k)
					{
						int y = towerTop + 1 + k;
						for(int x = 2 + k; x <= 6 - k; ++x)
							for(int z = k; z <= T - 1 - k; ++z)
								{
									bool e = x == 2 + k || x == 6 - k || z == k || z == T - 1 - k;
									if(!e)
										continue;
									bool corner = (x == 2 + k || x == 6 - k) && (z == k || z == T - 1 - k);
									if(corner)
										b.set(x, y, z, roofFull);
									else if(z == k)
										b.face(x, y, z, roof, 2);
									else if(z == T - 1 - k)
										b.face(x, y, z, roof, 0);
									else if(x == 2 + k)
										b.face(x, y, z, roof, 1);
									else
										b.face(x, y, z, roof, 3);
								}
					}
				b.set(4, towerTop + 4, 2, roofFull);
				b.set(4, towerTop + 5, 2, BlockType::Oak_Fence);
				b.set(4, towerTop + 6, 2, BlockType::Gold_Block);
				// Entrée, passage vers la nef, fenêtre au-dessus de la porte
				b.door(4, 1, 0, 0);
				b.set(4, 0, -1, base);
				for(int y = 1; y <= 3; ++y)
					b.clear(4, y, T - 1);
				b.set(4, 5, 0, BlockType::Red_Stained_Glass_Pane);
				b.set(4, 9, 0, BlockType::Glass_Pane);
				// Bancs (escaliers tournés vers l'autel), autel, lanternes
				for(int z = T + 2; z < D - 5; z += 2)
					for(int x : {1, 2, 3, 5, 6, 7})
						b.face(x, 1, z, BlockType::Oak_Stairs, 0);
				for(int x = 2; x <= 6; ++x)
					b.set(x, 1, D - 3, stone);
				b.set(4, 1, D - 3, BlockType::Gold_Block);
				b.set(2, 2, D - 3, BlockType::Torch);
				b.set(6, 2, D - 3, BlockType::Torch);
				for(int z = T + 3; z < D - 2; z += 5)
					b.set(4, 7, z, BlockType::Lantern);
				b.set(4, 3, 2, BlockType::Lantern);
			}

			// ── Forge : atelier de pierre, auvent, foyers ─────────
			void smithy(const Placed &pl)
			{
				Builder b{bp, pl.origin, pl.rot};
				const int w = 9, d = 9;
				BlockType stone = style == 4 ? BlockType::Sandstone : BlockType::Cobblestone;
				groundwork(b, -1, -1, w, d, stone, 12);
				for(int x = 0; x < w; ++x)
					for(int z = 0; z < d; ++z)
						b.set(x, 0, z, stone);
				// Pièce fermée (x 0..4)
				for(int y = 1; y <= 3; ++y)
					for(int x = 0; x <= 4; ++x)
						for(int z = 0; z < d; ++z)
							if(x == 0 || x == 4 || z == 0 || z == d - 1)
								b.set(x, y, z, ((x == 0 || x == 4) && (z == 0 || z == d - 1)) ? pal.frame : stone);
				b.door(2, 1, 0, 0);
				b.set(0, 2, 4, BlockType::Glass_Pane);
				b.set(2, 2, d - 1, BlockType::Glass_Pane);
				b.clear(4, 1, 4);
				b.clear(4, 2, 4); // passage vers l'atelier
				// Atelier ouvert (x 5..8) : poteaux, rambarde, mur du fond
				for(int z : {0, 4, d - 1})
					for(int y = 1; y <= 3; ++y)
						b.set(w - 1, y, z, pal.frame);
				for(int z = 1; z < d - 1; ++z)
					if(z != 4)
						b.set(w - 1, 1, z, pal.fence);
				for(int x = 5; x < w - 1; ++x)
					for(int y = 1; y <= 3; ++y)
						b.set(x, y, d - 1, stone);
				gableRoof(b, w, d, 4, style == 4 ? BlockType::Sandstone_Stairs : BlockType::Cobblestone_Stairs,
						  style == 4 ? BlockType::Sandstone : BlockType::Cobblestone, stone, false);
				// Foyer, cheminée, établi, tonneaux, bac d'eau
				b.set(6, 1, d - 2, BlockType::Forge);
				b.set(7, 1, d - 2, BlockType::Forge);
				for(int y = 2; y <= 10; ++y)
					b.set(7, y, d - 2, BlockType::Bricks);
				b.set(6, 2, d - 2, BlockType::Bricks);
				b.set(6, 1, 2, BlockType::Stone_Brick_Slab); // enclume
				b.set(5, 1, 1, BlockType::Barrel);
				b.set(5, 1, 2, BlockType::Barrel);
				b.set(1, 1, d - 2, BlockType::Crafting_Table);
				b.set(3, 1, d - 2, BlockType::Barrel);
				b.set(1, 1, 1, BlockType::Bookshelf);
				b.set(6, 1, 5, BlockType::Cobblestone);
				b.set(7, 1, 5, BlockType::Water);
				b.set(7, 0, 5, BlockType::Cobblestone);
				b.set(2, 3, 4, BlockType::Lantern);
				b.set(6, 3, 4, BlockType::Lantern);
			}

			// ── Bibliothèque : murs hauts, rayonnages, fenêtres hautes
			void library(const Placed &pl)
			{
				Builder b{bp, pl.origin, pl.rot};
				const int w = pl.w, d = pl.d, top = 5;
				groundwork(b, -1, -1, w, d, pal.foundation, top + w / 2 + 4);
				for(int x = 0; x < w; ++x)
					for(int z = 0; z < d; ++z)
						b.set(x, 0, z, (x + z) % 2 ? pal.floor : BlockType::Spruce_Planks);
				for(int y = 1; y <= top; ++y)
					for(int x = 0; x < w; ++x)
						for(int z = 0; z < d; ++z)
							{
								bool ex = x == 0 || x == w - 1, ez = z == 0 || z == d - 1;
								if(ex || ez)
									b.set(x, y, z, (ex && ez) || (pal.timber && y == top) ? pal.frame : pal.wall);
							}
				for(int z = 2; z < d - 2; z += 2)
					for(int y = 3; y <= 4; ++y)
						{
							b.set(0, y, z, BlockType::Glass_Pane);
							b.set(w - 1, y, z, BlockType::Glass_Pane);
						}
				for(int x = 2; x < w - 2; x += 2)
					if(x != w / 2)
						for(int y = 2; y <= 4; ++y)
							b.set(x, y, 0, BlockType::Glass_Pane);
				b.door(w / 2, 1, 0, 0);
				b.set(w / 2, 0, -1, pal.foundation);
				if(pal.flatRoofs)
					flatRoof(b, w, d, top + 1);
				else
					gableRoof(b, w, d, top + 1, pal.roof, pal.roofFull, pal.wall, false);
				// Rayonnages le long des murs, lutrin, lanternes
				for(int z = 1; z < d - 1; ++z)
					for(int y = 1; y <= 2; ++y)
						{
							b.set(1, y, z, BlockType::Bookshelf);
							b.set(w - 2, y, z, BlockType::Bookshelf);
						}
				for(int x = 2; x < w - 2; ++x)
					for(int y = 1; y <= 3; ++y)
						b.set(x, y, d - 2, BlockType::Bookshelf);
				b.set(w / 2, 1, d / 2, BlockType::Crafting_Table);
				b.face(w / 2, 1, d / 2 - 1, BlockType::Oak_Stairs, 0);
				b.set(w / 2, top, d / 2, BlockType::Lantern);
				b.set(w / 2, top, 2, BlockType::Lantern);
			}

			// ── Taverne : grande maison à étage, comptoir, enseigne ─
			void tavern(const Placed &pl)
			{
				house(pl, 2, true, true);
				Builder b{bp, pl.origin, pl.rot};
				const int w = pl.w, d = pl.d;
				for(int x = 2; x < w - 2; ++x)
					b.set(x, 1, d - 3, x % 3 == 0 ? BlockType::Barrel : BlockType::Spruce_Planks);
				b.set(1, 1, d - 2, BlockType::Barrel);
				b.set(w - 2, 1, d - 2, BlockType::Barrel);
				for(int x : {2, w - 3})
					{
						b.set(x, 1, 2, BlockType::Crafting_Table);
						b.face(x - 1, 1, 2, BlockType::Spruce_Stairs, 3);
						b.face(x + 1, 1, 2, BlockType::Spruce_Stairs, 1);
					}
				b.set(w / 2, 3, -1, BlockType::Yellow_Wool); // enseigne
				for(int x : {w / 2 - 2, w / 2 + 2})
					{
						glm::ivec3 p = b.w(x, 0, -1);
						int y = H(p.x, p.z) - pl.origin.y;
						b.set(x, std::max(y, 1), -1, BlockType::Barrel, Mode::Fill);
					}
				b.set(w / 2 - 1, 3, 1, BlockType::Lantern);
			}

			// ── Grange : grande porte, foin, enclos attenant ──────
			void barn(const Placed &pl)
			{
				Builder b{bp, pl.origin, pl.rot};
				const int w = pl.w, d = pl.d, top = 4;
				BlockType wall = style == 4 ? BlockType::Sandstone : BlockType::Spruce_Planks;
				BlockType frame = style == 4 ? BlockType::Chiseled_Sandstone : BlockType::Spruce_Log;
				groundwork(b, -1, -1, w, d, pal.foundation, top + w / 2 + 3);
				for(int x = 0; x < w; ++x)
					for(int z = 0; z < d; ++z)
						b.set(x, 0, z, (x == 0 || x == w - 1 || z == 0 || z == d - 1) ? pal.foundation
																					   : BlockType::Dirt_Path);
				for(int y = 1; y <= top; ++y)
					for(int x = 0; x < w; ++x)
						for(int z = 0; z < d; ++z)
							{
								bool ex = x == 0 || x == w - 1, ez = z == 0 || z == d - 1;
								if(ex || ez)
									b.set(x, y, z, (ex && ez) || (ex && z % 3 == 0) ? frame : wall);
							}
				for(int x = w / 2 - 1; x <= w / 2 + 1; ++x)
					for(int y = 1; y <= 3; ++y)
						b.clear(x, y, 0);
				gableRoof(b, w, d, top + 1, style == 4 ? BlockType::Sandstone_Stairs : BlockType::Spruce_Stairs,
						  style == 4 ? BlockType::Sandstone : BlockType::Spruce_Planks, wall, false);
				for(int z = 2; z < d - 1; z += 3)
					{
						b.set(1, 1, z, BlockType::Hay_Bale);
						b.set(1, 2, z, BlockType::Hay_Bale);
						b.set(w - 2, 1, z, BlockType::Hay_Bale);
					}
				b.set(w / 2, top, d / 2, BlockType::Lantern);
				// Enclos derrière la grange
				for(int x = -1; x <= w; ++x)
					for(int z = d; z <= d + 7; ++z)
						{
							bool edge = x == -1 || x == w || z == d + 7;
							int top2 = groundTop(b, x, z);
							b.set(x, top2 + 1, z, BlockType::Air, Mode::ClearSoft);
							if(edge && !(z == d + 7 && x == w / 2))
								b.set(x, top2 + 1, z, pal.fence);
						}
				b.set(2, groundTop(b, 2, d + 3) + 1, d + 3, BlockType::Hay_Bale);
			}

			// ── Moulin à vent : tour, ailes de toile ──────────────
			void windmill(const Placed &pl)
			{
				Builder b{bp, pl.origin, pl.rot};
				// Emprise 13 x 7 : tour 5 x 5 centrée en x 4..8, z 1..5 ; ailes en z = 0
				groundwork(b, 3, 0, 9, 6, BlockType::Cobblestone, 18);
				for(int x = 4; x <= 8; ++x)
					for(int z = 1; z <= 5; ++z)
						b.set(x, 0, z, BlockType::Cobblestone);
				for(int y = 1; y <= 10; ++y)
					for(int x = 4; x <= 8; ++x)
						for(int z = 1; z <= 5; ++z)
							{
								bool ex = x == 4 || x == 8, ez = z == 1 || z == 5;
								if(!ex && !ez)
									continue;
								BlockType m = y <= 3 ? BlockType::Cobblestone : pal.wall;
								if(ex && ez && y > 3)
									m = BlockType::Log;
								b.set(x, y, z, m);
							}
				b.door(6, 1, 1, 0);
				b.set(6, 6, 5, BlockType::Glass_Pane);
				b.set(4, 6, 3, BlockType::Glass_Pane);
				// Toit posé sur la tour (x 4..8) : repère local décalé
				Builder roofB{bp, b.w(4, 0, 0), pl.rot};
				gableRoof(roofB, 5, 5, 11, BlockType::Spruce_Stairs, BlockType::Spruce_Planks, pal.wall, false, 1, 1);
				// Moyeu et ailes
				b.set(6, 9, 0, BlockType::Log);
				for(int k = 1; k <= 5; ++k)
					{
						b.set(6, 9 + k, 0, BlockType::Spruce_Fence);
						b.set(7, 9 + k, 0, BlockType::White_Wool);
						b.set(6, 9 - k, 0, BlockType::Spruce_Fence);
						b.set(5, 9 - k, 0, BlockType::White_Wool);
						b.set(6 - k, 9, 0, BlockType::Spruce_Fence);
						b.set(6 - k, 8, 0, BlockType::White_Wool);
						b.set(6 + k, 9, 0, BlockType::Spruce_Fence);
						b.set(6 + k, 10, 0, BlockType::White_Wool);
					}
				b.set(6, 3, 3, BlockType::Lantern);
			}

			// ── Entrepôt du port ───────────────────────────────────
			void warehouse(const Placed &pl)
			{
				Builder b{bp, pl.origin, pl.rot};
				const int w = pl.w, d = pl.d;
				groundwork(b, -1, -1, w, d, pal.foundation, 10);
				for(int x = 0; x < w; ++x)
					for(int z = 0; z < d; ++z)
						b.set(x, 0, z, BlockType::Spruce_Planks);
				for(int y = 1; y <= 4; ++y)
					for(int x = 0; x < w; ++x)
						for(int z = 0; z < d; ++z)
							{
								bool ex = x == 0 || x == w - 1, ez = z == 0 || z == d - 1;
								if(ex || ez)
									b.set(x, y, z, (ex && ez) ? BlockType::Spruce_Log : BlockType::Spruce_Planks);
							}
				for(int x = w / 2 - 1; x <= w / 2 + 1; ++x)
					for(int y = 1; y <= 3; ++y)
						b.clear(x, y, 0);
				gableRoof(b, w, d, 5, BlockType::Spruce_Stairs, BlockType::Spruce_Planks, BlockType::Spruce_Planks,
						  w > d);
				for(int z = 1; z < d - 1; z += 2)
					{
						b.set(1, 1, z, BlockType::Barrel);
						b.set(1, 2, z, BlockType::Barrel);
						b.set(w - 2, 1, z, z % 4 == 1 ? BlockType::Hay_Bale : BlockType::Barrel);
					}
				b.set(w / 2, 4, d / 2, BlockType::Lantern);
			}

			// ── Étal de marché ─────────────────────────────────────
			void stall(int x, int z, int rot, int y)
			{
				Builder b{bp, {x, y, z}, rot};
				BlockType a = WOOL[1 + r.range(0, 3)], c = BlockType::White_Wool;
				for(int px : {0, 2})
					for(int pz : {0, 2})
						for(int yy = 1; yy <= 2; ++yy)
							b.set(px, yy, pz, pal.fence);
				for(int px = 0; px <= 2; ++px)
					for(int pz = 0; pz <= 2; ++pz)
						b.set(px, 3, pz, px % 2 ? c : a);
				b.set(1, 1, 0, BlockType::Oak_Slab);
				const BlockType goods[5] = {BlockType::Hay_Bale, BlockType::Pumpkin, BlockType::Barrel,
											BlockType::Bookshelf, BlockType::Wheat};
				b.set(1, 1, 1, goods[r.range(0, 4)]);
			}

			// ── Lampadaire ─────────────────────────────────────────
			void lamp(int x, int z, int y = -1)
			{
				if(y < 0)
					y = H(x, z);
				for(int k = 0; k < 3; ++k)
					bp.add({x, y + k, z}, pal.fence, Mode::Set);
				bp.add({x, y + 3, z}, BlockType::Lantern, Mode::Set);
			}

			// ═════════════════════════════════════════════════════
			void walkRoad(Road &road, glm::vec2 pos, glm::vec2 dir, int length, bool stopOnRoad)
			{
				float turn = 0.0f;
				int prevH = H(static_cast<int>(std::lround(pos.x)), static_cast<int>(std::lround(pos.y)));
				for(int step = 0; step < length; ++step)
					{
						pos += dir;
						glm::ivec2 c(static_cast<int>(std::lround(pos.x)), static_cast<int>(std::lround(pos.y)));
						if(!g.inside(c.x, c.y, 8))
							break;
						int h = H(c.x, c.y);
						if(h <= SEA_LEVEL)
							{
								road.water = true;
								road.waterDir = dir;
								break;
							}
						if(std::abs(h - prevH) > 2)
							break; // trop raide
						uint8_t here = g.get(c.x, c.y);
						if(here == BuildingCell)
							break;
						if(stopOnRoad && step > 3 && here == RoadCell)
							break; // jonction avec une autre rue
						if(road.pts.empty() || road.pts.back() != c)
							road.pts.push_back(c);
						prevH = h;
						int hw = road.halfWidth;
						for(int dz = -hw; dz <= hw; ++dz)
							for(int dx = -hw; dx <= hw; ++dx)
								if(g.get(c.x + dx, c.y + dz) == Free)
									g.set(c.x + dx, c.y + dz, RoadCell);
						turn = std::clamp(turn * 0.85f + (r.f() - 0.5f) * 0.07f, -0.07f, 0.07f);
						float cs = std::cos(turn), sn = std::sin(turn);
						dir = glm::normalize(glm::vec2(dir.x * cs - dir.y * sn, dir.x * sn + dir.y * cs));
					}
			}

			static glm::vec2 tangent(const Road &rd, size_t i)
			{
				size_t a = i >= 2 ? i - 2 : 0, b = std::min(rd.pts.size() - 1, i + 2);
				glm::vec2 t = glm::vec2(rd.pts[b] - rd.pts[a]);
				return glm::length(t) > 0.01f ? glm::normalize(t) : glm::vec2(1, 0);
			}

			// Emplacement d'un bâtiment le long d'une rue, porte face à la rue
			// Plusieurs reculs et décalages latéraux (rues en biais ou courbes)
			bool fit(const Slot &s, BType type, int w, int d, int tol, Placed &out, int frontGap = 2,
					 float maxDist = 1e9f, float minDist = 0.0f)
			{
				for(int gap = frontGap; gap <= frontGap + 2; ++gap)
					for(int shift : {0, -2, 2})
						if(fitAt(s, type, w, d, tol, out, gap, shift, maxDist, minDist))
							return true;
				return false;
			}

			bool fitAt(const Slot &s, BType type, int w, int d, int tol, Placed &out, int frontGap, int shift,
					   float maxDist, float minDist)
			{
				glm::vec2 n = glm::vec2(-s.t.y, s.t.x) * static_cast<float>(s.side);
				glm::ivec2 doorDir = std::abs(n.x) > std::abs(n.y) ? glm::ivec2(n.x > 0 ? -1 : 1, 0)
																   : glm::ivec2(0, n.y > 0 ? -1 : 1);
				int rot = doorDir.y < 0 ? 0 : doorDir.x > 0 ? 1 : doorDir.y > 0 ? 2 : 3;
				glm::vec2 front = s.p - glm::vec2(doorDir) * static_cast<float>(s.halfWidth + frontGap)
								  + s.t * static_cast<float>(shift);
				glm::ivec2 f(static_cast<int>(std::lround(front.x)), static_cast<int>(std::lround(front.y)));
				glm::ivec2 off = rotate2(w / 2, 0, rot);
				glm::ivec3 origin(f.x - off.x, 0, f.y - off.y);
				Builder b{bp, origin, rot};
				glm::ivec3 c0 = b.w(-1, 0, -1), c1 = b.w(w, 0, d);
				Rect rect{std::min(c0.x, c1.x), std::min(c0.z, c1.z), std::max(c0.x, c1.x), std::max(c0.z, c1.z)};
				// Dans l'enceinte (ou hors de l'enceinte pour les champs, moulins...)
				for(glm::ivec2 c : {glm::ivec2(rect.x0, rect.z0), glm::ivec2(rect.x1, rect.z1),
									glm::ivec2(rect.x0, rect.z1), glm::ivec2(rect.x1, rect.z0)})
					{
						float dc = glm::length(glm::vec2(c - glm::ivec2(cx, cz)));
						if(dc > maxDist || dc < minDist)
							return false;
					}
				// Un bloc d'écart avec les voisins (en plus des avancées de toit)
				Rect around{rect.x0 - 1, rect.z0 - 1, rect.x1 + 1, rect.z1 + 1};
				auto freeAround = [&]() {
					for(int z = around.z0; z <= around.z1; ++z)
						for(int x = around.x0; x <= around.x1; ++x)
							{
								uint8_t c = g.get(x, z);
								bool border = !rect.contains(x, z);
								if(c == BuildingCell || c == WallCell || c == 0xFF || (!border && c != Free))
									return false;
							}
					return true;
				};
				if(!g.inside(around.x0, around.z0, 4) || !g.inside(around.x1, around.z1, 4) || !freeAround())
					return false;
				int mn = 1 << 20, mx = -(1 << 20);
				for(int x = 0; x < w; x += std::max(1, w / 4))
					for(int z = 0; z < d; z += std::max(1, d / 4))
						{
							glm::ivec3 p = b.w(x, 0, z);
							int h = H(p.x, p.z);
							mn = std::min(mn, h);
							mx = std::max(mx, h);
						}
				for(glm::ivec3 p : {b.w(w - 1, 0, d - 1), b.w(0, 0, d - 1), b.w(w - 1, 0, 0)})
					{
						int h = H(p.x, p.z);
						mn = std::min(mn, h);
						mx = std::max(mx, h);
					}
				if(mn <= SEA_LEVEL + 1 || mx - mn > tol)
					return false;
				origin.y = mx - 1;
				g.fill(rect, BuildingCell);
				// Allée entre la porte et la rue
				glm::ivec3 step = b.w(w / 2, 0, -1);
				for(int k = 0; k <= frontGap; ++k)
					{
						glm::ivec2 c = glm::ivec2(step.x, step.z) + doorDir * k;
						if(g.get(c.x, c.y) == Free || g.get(c.x, c.y) == BuildingCell)
							g.set(c.x, c.y, RoadCell);
					}
				out = {type, origin, rot, w, d, rect};
				return true;
			}

			// ── Champ le long d'une rue (sans bordure de rondins) ──
			bool field(const Slot &s)
			{
				int len = r.range(9, 17), dep = r.range(6, 12);
				glm::vec2 n = glm::vec2(-s.t.y, s.t.x) * static_cast<float>(s.side);
				bool alongX = std::abs(s.t.x) > std::abs(s.t.y);
				int sx = alongX ? len : dep, sz = alongX ? dep : len;
				glm::vec2 c = s.p + n * (s.halfWidth + 1.5f + dep * 0.5f);
				Rect rect{static_cast<int>(c.x) - sx / 2, static_cast<int>(c.y) - sz / 2,
						  static_cast<int>(c.x) - sx / 2 + sx - 1, static_cast<int>(c.y) - sz / 2 + sz - 1};
				if(!g.inside(rect.x0, rect.z0, 4) || !g.inside(rect.x1, rect.z1, 4) || !g.free(rect))
					return false;
				std::vector<int> hs;
				for(int z = rect.z0; z <= rect.z1; ++z)
					for(int x = rect.x0; x <= rect.x1; ++x)
						hs.push_back(H(x, z));
				auto [mnIt, mxIt] = std::minmax_element(hs.begin(), hs.end());
				if(*mnIt <= SEA_LEVEL + 1 || *mxIt - *mnIt > 2)
					return false;
				std::nth_element(hs.begin(), hs.begin() + hs.size() / 2, hs.end());
				int fy = hs[hs.size() / 2] - 1;
				g.fill(rect, FieldCell);

				BlockType crop = CROPS[r.range(0, 3)];
				if(style == 4 && crop == BlockType::Beetroots)
					crop = BlockType::Wheat;
				bool fenced = r.chance(0.35f), scarecrow = r.chance(0.3f);
				for(int z = rect.z0; z <= rect.z1; ++z)
					for(int x = rect.x0; x <= rect.x1; ++x)
						{
							int h = H(x, z) - 1;
							for(int y = fy + 1; y <= std::max(h, fy) + 3; ++y)
								bp.add({x, y, z}, BlockType::Air, Mode::Clear);
							for(int y = h; y < fy; ++y)
								bp.add({x, y, z}, BlockType::Dirt, Mode::Set);
							// Rigoles d'irrigation parallèles à la rue, une rangée sur cinq
							int row = alongX ? z - rect.z0 : x - rect.x0;
							bool channel = row % 5 == 2;
							bp.add({x, fy, z}, channel ? BlockType::Water : BlockType::Farmland, Mode::Set);
							if(!channel)
								bp.add({x, fy + 1, z}, crop, Mode::Set);
							bool edge = x == rect.x0 || x == rect.x1 || z == rect.z0 || z == rect.z1;
							// Clôture sur les côtés éloignés de la rue
							glm::vec2 toRoad = s.p - glm::vec2(x, z);
							bool roadSide = glm::dot(toRoad, n) > 0.0f && glm::length(toRoad) < dep + s.halfWidth + 3;
							if(fenced && edge && !roadSide && !channel)
								bp.add({x, fy + 1, z}, pal.fence, Mode::Set);
						}
				if(scarecrow)
					{
						int x = (rect.x0 + rect.x1) / 2, z = (rect.z0 + rect.z1) / 2;
						bp.add({x, fy + 1, z}, pal.fence, Mode::Set);
						bp.add({x, fy + 2, z}, BlockType::Hay_Bale, Mode::Set);
						bp.add({x, fy + 3, z}, BlockType::Jack_O_Lantern, Mode::Set);
						bp.add({x - 1, fy + 2, z}, pal.fence, Mode::Set);
						bp.add({x + 1, fy + 2, z}, pal.fence, Mode::Set);
					}
				if(r.chance(0.5f))
					{
						glm::ivec2 k(rect.x0, rect.z0);
						bp.add({k.x, fy + 1, k.y}, BlockType::Hay_Bale, Mode::Set);
						bp.add({k.x + 1, fy + 1, k.y}, BlockType::Hay_Bale, Mode::Set);
						bp.add({k.x, fy + 2, k.y}, BlockType::Hay_Bale, Mode::Set);
					}
				return true;
			}

			// ── Remparts : anneau de pierre, tours, portes ────────
			void ramparts()
			{
				BlockType stone = style == 4 ? BlockType::Sandstone : BlockType::Stone_Bricks;
				BlockType alt = style == 4 ? BlockType::Chiseled_Sandstone
										   : (style == 1 ? BlockType::Mossy_Cobblestone : BlockType::Mossy_Stone_Bricks);
				const float Rw = static_cast<float>(wallRadius);
				int ext = wallRadius + 3;
				// Tours régulièrement espacées sur l'anneau
				int towers = std::max(6, static_cast<int>(2.0f * PI * Rw / 30.0f));
				float a0 = r.f() * PI;
				std::vector<glm::vec2> towerPos;
				for(int i = 0; i < towers; ++i)
					{
						float a = a0 + i * 2.0f * PI / towers;
						towerPos.push_back(glm::vec2(cx + std::cos(a) * Rw, cz + std::sin(a) * Rw));
					}
				for(int dz = -ext; dz <= ext; ++dz)
					for(int dx = -ext; dx <= ext; ++dx)
						{
							float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz));
							int x = cx + dx, z = cz + dz;
							bool towerCell = false, towerCenter = false;
							for(const glm::vec2 &t : towerPos)
								{
									float dt = glm::length(glm::vec2(x, z) - t);
									towerCell |= dt <= 2.6f;
									towerCenter |= dt < 0.71f;
								}
							bool wallCell = dist >= Rw - 0.5f && dist <= Rw + 1.0f;
							if(!wallCell && !towerCell)
								continue;
							int h = H(x, z);
							if(h <= SEA_LEVEL)
								continue;
							uint8_t cell = g.get(x, z);
							bool gate = cell == RoadCell || cell == PlazaCell;
							if(cell == BuildingCell || cell == FieldCell || cell == DockCell)
								continue;
							if(gate && towerCell)
								towerCell = false;
							int top = towerCell ? 9 : 5;
							for(int y = h; y <= h + top + 1; ++y)
								bp.add({x, y, z}, BlockType::Air, Mode::Clear);
							bp.add({x, h - 1, z}, stone, Mode::Set);
							for(int y = gate ? h + 4 : h; y < h + top; ++y)
								bp.add({x, y, z}, ((x * 7 + y * 3 + z * 5) % 9 == 0) ? alt : stone, Mode::Set);
							// Créneaux (une pierre sur deux sur le bord extérieur)
							bool outer = towerCell ? true : dist > Rw + 0.2f;
							if(outer && (((x + z) & 1) == 0))
								bp.add({x, h + top, z}, stone, Mode::Set);
							if(towerCell && towerCenter)
								bp.add({x, h + top, z}, BlockType::Lantern, Mode::Set);
							if(!gate)
								g.set(x, z, WallCell);
						}
				// Lanternes aux portes : sur la rue principale, juste à l'intérieur
				for(const Road &rd : roads)
					{
						if(!rd.main)
							continue;
						for(size_t i = 1; i < rd.pts.size(); ++i)
							{
								float d0 = glm::length(glm::vec2(rd.pts[i - 1] - glm::ivec2(cx, cz)));
								float d1 = glm::length(glm::vec2(rd.pts[i] - glm::ivec2(cx, cz)));
								if(d0 < Rw - 3.0f && d1 >= Rw - 3.0f)
									{
										glm::vec2 t = tangent(rd, i);
										glm::vec2 n(-t.y, t.x);
										for(int sgn : {-1, 1})
											{
												glm::vec2 p = glm::vec2(rd.pts[i]) + n * (rd.halfWidth + 1.0f) * static_cast<float>(sgn);
												glm::ivec2 c(static_cast<int>(std::lround(p.x)), static_cast<int>(std::lround(p.y)));
												if(g.get(c.x, c.y) == Free)
													{
														lamp(c.x, c.y);
														g.set(c.x, c.y, BuildingCell);
													}
											}
										break;
									}
							}
					}
			}

			// ── Port : quai, pontons, barques ─────────────────────
			glm::ivec2 harborAt{0}, harborAxis{0};
			void harbor(const Road &rd)
			{
				glm::ivec2 shore = rd.pts.back();
				glm::ivec2 axis = std::abs(rd.waterDir.x) > std::abs(rd.waterDir.y)
									? glm::ivec2(rd.waterDir.x > 0 ? 1 : -1, 0)
									: glm::ivec2(0, rd.waterDir.y > 0 ? 1 : -1);
				glm::ivec2 side(-axis.y, axis.x);
				// Vérifie qu'il y a assez d'eau devant
				int water = 0;
				for(int k = 2; k <= 22; ++k)
					{
						glm::ivec2 p = shore + axis * k;
						water += H(p.x, p.y) <= SEA_LEVEL - 1;
					}
				if(water < 15)
					return;
				harborAt = shore;
				harborAxis = axis;
				const int deck = SEA_LEVEL; // pontons juste au-dessus de l'eau
				auto column = [&](glm::ivec2 p, BlockType top, BlockType support, bool post) {
					if(!g.inside(p.x, p.y, 2))
						return;
					int h = H(p.x, p.y);
					for(int y = deck + 1; y <= deck + 3; ++y)
						bp.add({p.x, y, p.y}, BlockType::Air, Mode::ClearSoft);
					if(post)
						for(int y = std::min(h, deck) - 1; y < deck; ++y)
							bp.add({p.x, y, p.y}, support, Mode::Set);
					bp.add({p.x, deck, p.y}, top, Mode::Set);
					g.set(p.x, p.y, DockCell);
				};
				// Quai de pierre le long du rivage
				for(int a = -7; a <= 7; ++a)
					for(int k = 0; k <= 2; ++k)
						column(shore + side * a + axis * k, BlockType::Stone_Bricks, BlockType::Stone_Bricks, true);
				// Pontons
				int piers = r.range(2, 3);
				for(int p = 0; p < piers; ++p)
					{
						int offset = piers == 2 ? (p == 0 ? -4 : 4) : (p - 1) * 7;
						int len = r.range(12, 20);
						for(int k = 3; k <= len; ++k)
							for(int a = -1; a <= 1; ++a)
								{
									glm::ivec2 c = shore + side * (offset + a) + axis * k;
									if(H(c.x, c.y) > SEA_LEVEL)
										continue;
									bool post = std::abs(a) == 1 && k % 4 == 0;
									column(c, BlockType::Spruce_Planks, BlockType::Spruce_Log, post);
								}
						// Lanterne en bout de ponton, barque amarrée
						glm::ivec2 end = shore + side * offset + axis * len;
						bp.add({end.x, deck + 1, end.y}, BlockType::Spruce_Fence, Mode::Set);
						bp.add({end.x, deck + 2, end.y}, BlockType::Lantern, Mode::Set);
						boat(shore + side * (offset + 3) + axis * (len - 6), axis, side, r.chance(0.5f));
					}
				// Tonneaux sur le quai
				for(int a = -6; a <= 6; a += 4)
					{
						glm::ivec2 c = shore + side * a + axis * 1;
						bp.add({c.x, deck + 1, c.y}, BlockType::Barrel, Mode::Set);
					}
			}

			void boat(glm::ivec2 at, glm::ivec2 axis, glm::ivec2 side, bool sail)
			{
				const int deck = SEA_LEVEL;
				for(int k = 0; k < 7; ++k)
					{
						int half = (k == 0 || k == 6) ? 0 : 1;
						for(int a = -half; a <= half; ++a)
							{
								glm::ivec2 c = at + axis * k + side * a;
								if(H(c.x, c.y) > SEA_LEVEL - 1)
									return; // échouée : pas de barque
								bp.add({c.x, deck - 1, c.y}, BlockType::Spruce_Planks, Mode::Set);
								bool rim = std::abs(a) == half;
								bp.add({c.x, deck, c.y}, rim ? BlockType::Spruce_Slab : BlockType::Air,
									   rim ? Mode::Set : Mode::Clear);
							}
					}
				if(sail)
					{
						glm::ivec2 m = at + axis * 3;
						for(int y = deck; y <= deck + 4; ++y)
							bp.add({m.x, y, m.y}, BlockType::Spruce_Fence, Mode::Set);
						for(int y = deck + 2; y <= deck + 4; ++y)
							for(int k = 1; k <= 2; ++k)
								{
									glm::ivec2 c = m + axis * k;
									bp.add({c.x, y, c.y}, BlockType::White_Wool, Mode::Set);
								}
					}
			}

			// ═════════════════════════════════════════════════════
			void plan()
			{
				// ── Place centrale ──
				plazaR = 6 + r.range(0, 2);
				plazaY = H(cx, cz) - 1;
				for(int dz = -plazaR; dz <= plazaR; ++dz)
					for(int dx = -plazaR; dx <= plazaR; ++dx)
						if(dx * dx + dz * dz <= plazaR * plazaR + plazaR)
							g.set(cx + dx, cz + dz, PlazaCell);

				// ── Rues principales ──
				int nMain = r.range(3, 5);
				float a0 = r.f() * 2.0f * PI;
				for(int i = 0; i < nMain; ++i)
					{
						float a = a0 + i * 2.0f * PI / nMain + (r.f() - 0.5f) * 0.6f;
						glm::vec2 dir(std::cos(a), std::sin(a));
						Road rd;
						rd.main = true;
						rd.halfWidth = 1;
						walkRoad(rd, glm::vec2(cx, cz) + dir * static_cast<float>(plazaR - 1), dir,
								 static_cast<int>(R * 0.95f), false);
						if(rd.pts.size() > 4)
							roads.push_back(rd);
					}
				// ── Ruelles ──
				size_t mains = roads.size();
				for(size_t ri = 0; ri < mains; ++ri)
					{
						Road base = roads[ri];
						for(size_t i = 12; i + 6 < base.pts.size(); i += static_cast<size_t>(r.range(12, 20)))
							{
								glm::vec2 t = tangent(base, i);
								float sgn = r.chance(0.5f) ? 1.0f : -1.0f;
								glm::vec2 dir = glm::normalize(glm::vec2(-t.y, t.x) * sgn + t * (r.f() - 0.5f) * 0.6f);
								Road rd;
								rd.halfWidth = 0;
								walkRoad(rd, glm::vec2(base.pts[i]) + dir * 2.0f, dir, r.range(16, 40), true);
								if(rd.pts.size() > 6)
									roads.push_back(rd);
								// Parfois une ruelle de l'autre côté aussi
								if(r.chance(0.4f))
									{
										Road rd2;
										rd2.halfWidth = 0;
										glm::vec2 dir2 = -dir;
										walkRoad(rd2, glm::vec2(base.pts[i]) + dir2 * 2.0f, dir2, r.range(14, 32), true);
										if(rd2.pts.size() > 6)
											roads.push_back(rd2);
									}
							}
					}

				// ── Emplacements le long des rues ──
				for(const Road &rd : roads)
					for(size_t i = 2; i < rd.pts.size(); i += 2)
						for(int side : {-1, 1})
							slots.push_back({glm::vec2(rd.pts[i]), tangent(rd, i), side, rd.halfWidth,
											 glm::length(glm::vec2(rd.pts[i] - glm::ivec2(cx, cz)))});
				std::sort(slots.begin(), slots.end(), [](const Slot &a, const Slot &b) { return a.dist < b.dist; });

				// ── Remparts décidés d'avance : la ville se serre à l'intérieur,
				// champs et moulins à l'extérieur ──
				bool walls = R >= 90 && (style != 2 ? r.chance(0.65f) : r.chance(0.35f));
				if(walls)
					{
						wallRadius = static_cast<int>(R * 0.62f);
						for(int dz = -wallRadius - 3; dz <= wallRadius + 3; ++dz)
							for(int dx = -wallRadius - 3; dx <= wallRadius + 3; ++dx)
								{
									float dc = std::sqrt(static_cast<float>(dx * dx + dz * dz));
									if(dc >= wallRadius - 1.5f && dc <= wallRadius + 2.5f
									   && g.get(cx + dx, cz + dz) == Free)
										g.set(cx + dx, cz + dz, WallCell);
								}
					}
				const float core = walls ? wallRadius - 4.0f : R * 0.75f;

				// ── Bâtiments remarquables, au plus près de la place ──
				struct Landmark
				{
					BType type;
					int w, d, tol;
				};
				std::vector<Landmark> landmarks = {{BType::Church, 9, 22, 6}, {BType::Smithy, 9, 9, 5},
												   {BType::Tavern, 11, 9, 5},  {BType::Library, 9, 9, 5}};
				if(R > 110)
					landmarks.push_back({BType::Smithy, 9, 9, 5});
				if(R > 120)
					landmarks.push_back({BType::Tavern, 11, 9, 5});
				for(const Landmark &lm : landmarks)
					for(const Slot &s : slots)
						{
							Placed p;
							if(s.dist < core && fit(s, lm.type, lm.w, lm.d, lm.tol, p, 2, core))
								{
									placed.push_back(p);
									break;
								}
						}
				// ── Maisons : d'abord les grandes et moyennes, puis les petites
				// comblent les vides ──
				for(int pass = 0; pass < 3; ++pass)
					for(const Slot &s : slots)
						{
							if(s.dist > core)
								continue;
							float t = s.dist / core;
							Placed p;
							if(pass == 0)
								{
									bool large = r.f() < 0.6f - 0.35f * t;
									int w = large ? (r.chance(0.5f) ? 7 : 9) : 7;
									int d = large ? (r.chance(0.5f) ? 9 : 11) : (r.chance(0.5f) ? 7 : 9);
									if(fit(s, large ? BType::HouseLarge : BType::HouseMedium, w, d, 6, p, 2, core)
									   || (large && fit(s, BType::HouseLarge, 7, 9, 6, p, 2, core)))
										placed.push_back(p);
								}
							else if(pass == 1)
								{
									if(r.chance(0.7f) && (fit(s, BType::HouseMedium, 7, 7, 5, p, 2, core)
														  || fit(s, BType::HouseSmall, 5, 7, 5, p, 2, core)))
										placed.push_back(p);
								}
							else if(fit(s, BType::HouseSmall, 5, 7, 5, p, 2, core)
									|| fit(s, BType::HouseSmall, 5, 5, 5, p, 2, core))
								placed.push_back(p);
						}

				// ── Champs, moulin, grange le long des chemins extérieurs ──
				float fieldStart = walls ? wallRadius + 3.0f : R * 0.55f;
				int mills = 0, barns = 0;
				for(const Slot &s : slots)
					{
						if(s.dist < fieldStart)
							continue;
						Placed p;
						if(mills < (R > 110 ? 2 : 1) && r.chance(0.2f)
						   && fit(s, BType::Windmill, 13, 7, 3, p, 1, 1e9f, fieldStart))
							{
								placed.push_back(p);
								mills++;
								continue;
							}
						if(barns < 2 && r.chance(0.2f) && fit(s, BType::Barn, 9, 11, 4, p, 2, 1e9f, fieldStart))
							{
								// Réserve l'enclos derrière la grange
								Builder b{bp, p.origin, p.rot};
								glm::ivec3 e0 = b.w(-1, 0, p.d), e1 = b.w(p.w, 0, p.d + 8);
								Rect pen{std::min(e0.x, e1.x), std::min(e0.z, e1.z), std::max(e0.x, e1.x),
										 std::max(e0.z, e1.z)};
								if(g.free(pen))
									{
										g.fill(pen, BuildingCell);
										placed.push_back(p);
										barns++;
									}
								else
									g.fill(p.rect, Free);
								continue;
							}
						if(r.chance(0.7f))
							field(s);
					}

				// ── Port ──
				for(const Road &rd : roads)
					if(rd.water && rd.pts.size() > 8)
						{
							harbor(rd);
							// Entrepôt près du quai
							for(const Slot &s : slots)
								if(glm::length(s.p - glm::vec2(rd.pts.back())) < 14.0f)
									{
										Placed p;
										if(fit(s, BType::Warehouse, 9, 7, 3, p))
											{
												placed.push_back(p);
												break;
											}
									}
							break;
						}
				if(wallRadius > 0)
					ramparts();
			}

			// ═════════════════════════════════════════════════════
			void emit()
			{
				// ── Place : pavage à plat, fontaine ou puits, étals ──
				for(int dz = -plazaR - 1; dz <= plazaR + 1; ++dz)
					for(int dx = -plazaR - 1; dx <= plazaR + 1; ++dx)
						{
							int x = cx + dx, z = cz + dz;
							if(g.get(x, z) != PlazaCell)
								continue;
							int h = H(x, z) - 1;
							for(int y = plazaY + 1; y <= std::max(h, plazaY) + 4; ++y)
								bp.add({x, y, z}, BlockType::Air, Mode::Clear);
							for(int y = h; y < plazaY; ++y)
								bp.add({x, y, z}, pal.foundation, Mode::Set);
							bool ring = (std::abs(dx) + std::abs(dz)) % 3 == 0;
							bp.add({x, plazaY, z}, ring ? pal.plaza2 : pal.plaza, Mode::Set);
						}
				Builder c{bp, {cx, plazaY, cz}, 0};
				if(r.chance(0.6f))
					{
						// Fontaine
						for(int x = -2; x <= 2; ++x)
							for(int z = -2; z <= 2; ++z)
								{
									bool rim = std::abs(x) == 2 || std::abs(z) == 2;
									c.set(x, 1, z, rim ? BlockType::Stone_Bricks : BlockType::Water);
									c.set(x, 0, z, BlockType::Stone_Bricks);
								}
						for(int y = 1; y <= 3; ++y)
							c.set(0, y, 0, BlockType::Stone_Bricks);
						c.set(0, 4, 0, BlockType::Water);
						for(int k = 0; k < 4; ++k)
							{
								glm::ivec2 d = facingDir(k);
								c.face(d.x, 4, d.y, BlockType::Stone_Brick_Stairs, (k + 2) & 3);
							}
						c.set(2, 2, 2, BlockType::Lantern);
						c.set(-2, 2, -2, BlockType::Lantern);
					}
				else
					{
						// Puits couvert
						for(int x = -1; x <= 2; ++x)
							for(int z = -1; z <= 2; ++z)
								{
									bool inner = x >= 0 && x <= 1 && z >= 0 && z <= 1;
									c.set(x, -3, z, BlockType::Cobblestone);
									for(int y = -2; y <= 1; ++y)
										c.set(x, y, z, inner ? BlockType::Water : BlockType::Cobblestone);
								}
						for(int x : {-1, 2})
							for(int z : {-1, 2})
								for(int y = 2; y <= 3; ++y)
									c.set(x, y, z, pal.fence);
						for(int x = -1; x <= 2; ++x)
							for(int z = -1; z <= 2; ++z)
								c.set(x, 4, z, BlockType::Spruce_Slab);
						c.set(0, 4, 0, BlockType::Lantern);
					}
				// Étals autour de la fontaine
				int stalls = r.range(3, 6);
				for(int i = 0; i < stalls; ++i)
					{
						float a = r.f() * 2.0f * PI;
						int sx = cx + static_cast<int>(std::cos(a) * (plazaR - 2.5f)) - 1;
						int sz = cz + static_cast<int>(std::sin(a) * (plazaR - 2.5f)) - 1;
						if(std::abs(sx + 1 - cx) <= 3 && std::abs(sz + 1 - cz) <= 3)
							continue;
						int rot = std::abs(std::cos(a)) > std::abs(std::sin(a)) ? (std::cos(a) > 0 ? 1 : 3)
																				   : (std::sin(a) > 0 ? 2 : 0);
						stall(sx, sz, rot, plazaY);
					}
				for(int k = 0; k < 4; ++k)
					{
						float a = k * PI * 0.5f + PI * 0.25f;
						lamp(cx + static_cast<int>(std::cos(a) * (plazaR - 0.5f)),
							 cz + static_cast<int>(std::sin(a) * (plazaR - 0.5f)), plazaY + 1);
					}

				// ── Rues : chemin de terre battue, un peu de gravier ──
				for(int z = g.z0; z < g.z0 + g.size; ++z)
					for(int x = g.x0; x < g.x0 + g.size; ++x)
						{
							if(g.get(x, z) != RoadCell)
								continue;
							int h = H(x, z);
							if(h <= SEA_LEVEL)
								continue;
							uint32_t hsh = gen.hash(x, z, 0x0AD5u) % 100u;
							BlockType surf = hsh < 12 ? BlockType::Gravel : BlockType::Dirt_Path;
							if(style == 4 && hsh < 30)
								surf = BlockType::Sandstone;
							bp.add({x, h - 1, z}, surf, Mode::Set);
							bp.add({x, h, z}, BlockType::Air, Mode::ClearSoft);
							bp.add({x, h + 1, z}, BlockType::Air, Mode::ClearSoft);
						}

				// ── Bâtiments ──
				for(const Placed &p : placed)
					switch(p.type)
						{
						case BType::Church: church(p); break;
						case BType::Smithy: smithy(p); break;
						case BType::Tavern: tavern(p); break;
						case BType::Library: library(p); break;
						case BType::Barn: barn(p); break;
						case BType::Windmill: windmill(p); break;
						case BType::Warehouse: warehouse(p); break;
						case BType::HouseLarge: house(p, 2, r.chance(0.5f)); break;
						case BType::HouseMedium: house(p, 1, r.chance(0.5f)); break;
						default: house(p, 1, false); break;
						}

				// ── Lampadaires le long des rues principales ──
				for(const Road &rd : roads)
					{
						if(!rd.main)
							continue;
						for(size_t i = 6; i < rd.pts.size(); i += 11)
							{
								glm::vec2 t = tangent(rd, i);
								float sgn = (i / 11) % 2 ? 1.0f : -1.0f;
								glm::vec2 p = glm::vec2(rd.pts[i]) + glm::vec2(-t.y, t.x) * (rd.halfWidth + 1.0f) * sgn;
								glm::ivec2 q(static_cast<int>(std::lround(p.x)), static_cast<int>(std::lround(p.y)));
								if(g.get(q.x, q.y) == Free && H(q.x, q.y) > SEA_LEVEL)
									{
										lamp(q.x, q.y);
										g.set(q.x, q.y, BuildingCell);
									}
							}
					}

				// ── Masque sans arbres : tout ce qui est occupé, élargi ──
				const int dil = 3;
				bp.maskX0 = g.x0;
				bp.maskZ0 = g.z0;
				bp.maskSize = g.size;
				std::vector<uint8_t> rowPass(g.c.size(), 0);
				for(int z = 0; z < g.size; ++z)
					for(int x = 0; x < g.size; ++x)
						{
							bool any = false;
							for(int k = std::max(0, x - dil); k <= std::min(g.size - 1, x + dil) && !any; ++k)
								any = g.c[static_cast<size_t>(z) * g.size + k] != Free;
							rowPass[static_cast<size_t>(z) * g.size + x] = any;
						}
				bp.noTree.assign(g.c.size(), 0);
				for(int z = 0; z < g.size; ++z)
					for(int x = 0; x < g.size; ++x)
						{
							bool any = false;
							for(int k = std::max(0, z - dil); k <= std::min(g.size - 1, z + dil) && !any; ++k)
								any = rowPass[static_cast<size_t>(k) * g.size + x];
							// Dans l'enceinte (ou le coeur du village) : quelques arbres seulement
							int wx = g.x0 + x, wz = g.z0 + z;
							float dc = glm::length(glm::vec2(wx - cx, wz - cz));
							float core = wallRadius > 0 ? wallRadius + 2.0f : R * 0.6f;
							if(dc < core && gen.hash(wx, wz, 0x7AEEu) % 6u != 0u)
								any = true;
							bp.noTree[static_cast<size_t>(z) * g.size + x] = any;
						}
			}
		};
	}

namespace village
	{
		void build(const TerrainGenerator &gen, const StructureSite &site, StructureBlueprint &bp)
		{
			Village v(gen, bp, site);
			v.plan();
			v.emit();
		}
	}
