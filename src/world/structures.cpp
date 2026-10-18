// -----------------------------------------------------------------------------
// Fichier : structures.cpp
// Rôle : Choix des emplacements et plans des structures générées.
// Les bâtiments sont décrits en coordonnées locales (porte côté -Z) puis
// tournés vers leur orientation finale (Builder).
// -----------------------------------------------------------------------------
#include "structures.h"

#include "terrain_generator.h"

#include <algorithm>
#include <cmath>
#include <set>

using Mode = StructureBlueprint::Mode;

namespace
	{
		constexpr float PI = 3.14159265f;

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
		};

		uint64_t regionKey(int rx, int rz)
		{
			return (static_cast<uint64_t>(static_cast<uint32_t>(rx)) << 32)
				   | static_cast<uint32_t>(rz);
		}

		// Pose de blocs en coordonnées locales tournées (rot : 0..3, porte
		// locale vers -Z ; 1 : +X, 2 : +Z, 3 : -X)
		struct Builder
		{
			StructureBlueprint &bp;
			glm::ivec3 origin{0};
			int rot = 0;

			glm::ivec3 w(int x, int y, int z) const
			{
				switch(rot & 3)
					{
					case 0: return origin + glm::ivec3(x, y, z);
					case 1: return origin + glm::ivec3(-z, y, x);
					case 2: return origin + glm::ivec3(-x, y, -z);
					default: return origin + glm::ivec3(z, y, -x);
					}
			}
			void set(int x, int y, int z, BlockType t, Mode m = Mode::Set)
			{
				bp.add(w(x, y, z), t, m);
			}
			void clear(int x, int y, int z) { bp.add(w(x, y, z), BlockType::Air, Mode::Clear); }
		};

		// Décalage tourné (sans origine)
		glm::ivec2 rotate2(int x, int z, int rot)
		{
			switch(rot & 3)
				{
				case 0: return {x, z};
				case 1: return {-z, x};
				case 2: return {-x, -z};
				default: return {z, -x};
				}
		}

		struct Rect
		{
			int x0, z0, x1, z1;
			bool overlaps(const Rect &o, int pad) const
			{
				return x0 - pad <= o.x1 && x1 + pad >= o.x0 && z0 - pad <= o.z1 && z1 + pad >= o.z0;
			}
			bool contains(int x, int z) const { return x >= x0 && x <= x1 && z >= z0 && z <= z1; }
		};

		// Matériaux d'un style de village
		struct Style
		{
			BlockType wall, frame, floor, roof, foundation;
			bool flatRoof = false;
		};

		Style villageStyle(int style)
		{
			switch(style)
				{
				case 1: // taïga
					return {BlockType::Spruce_Planks, BlockType::Spruce_Log, BlockType::Spruce_Planks,
							BlockType::Mossy_Cobblestone, BlockType::Cobblestone};
				case 2: // cerisiers
					return {BlockType::White_Plaster, BlockType::Vermilion_Planks, BlockType::Cherry_Planks,
							BlockType::Spruce_Planks, BlockType::Stone_Bricks};
				case 3: // neige
					return {BlockType::Spruce_Planks, BlockType::Spruce_Log, BlockType::Plank,
							BlockType::Snow, BlockType::Cobblestone};
				case 4: // désert
					return {BlockType::Sandstone, BlockType::Chiseled_Sandstone, BlockType::Sandstone,
							BlockType::Sandstone, BlockType::Sandstone, true};
				default: // plaines : colombages
					return {BlockType::White_Plaster, BlockType::Log, BlockType::Plank,
							BlockType::Spruce_Planks, BlockType::Cobblestone};
				}
		}
	}

const char *structureName(StructureType type)
{
	switch(type)
		{
		case StructureType::Village: return "Village";
		case StructureType::DesertTemple: return "Desert temple";
		case StructureType::Ruins: return "Ruined tower";
		case StructureType::Igloo: return "Igloo";
		case StructureType::Shrine: return "Cherry shrine";
		case StructureType::Lighthouse: return "Lighthouse";
		default: return "None";
		}
}

// ── Emplacements ────────────────────────────────────────────────
bool StructureGenerator::siteInRegion(int rx, int rz, StructureSite &out) const
{
	uint64_t key = regionKey(rx, rz);
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_sites.find(key);
		if(it != m_sites.end())
			{
				out = it->second;
				return out.type != StructureType::None;
			}
	}
	StructureSite site = computeSite(rx, rz);
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_sites.emplace(key, site);
	}
	out = site;
	return site.type != StructureType::None;
}

StructureSite StructureGenerator::computeSite(int rx, int rz) const
{
	StructureSite none;
	uint32_t h = m_gen.hash(rx, rz, 0x57A1u);
	if(h % 100u >= 62u)
		return none;

	const int margin = 52;
	int cx = rx * REGION + margin + static_cast<int>((h >> 8) % (REGION - 2 * margin));
	int cz = rz * REGION + margin + static_cast<int>((h >> 16) % (REGION - 2 * margin));
	if(!m_gen.isAmplifiedWorld() && m_gen.amplifiedMask(cx, cz) > 0.02f)
		return none; // relief trop tourmenté (les plats sont vérifiés plus bas)

	ColumnInfo col = m_gen.getColumn(cx, cz);
	if(col.height <= SEA_LEVEL + 1)
		return none;
	auto H = [&](int x, int z) { return m_gen.getHeightAt(x, z); };
	int roll = static_cast<int>(m_gen.hash(rx, rz, 0x0B1Eu) % 100u);

	StructureSite s;
	s.center = glm::ivec3(cx, col.height, cz);
	s.seed = m_gen.hash(rx, rz, 0xBEEFu);
	switch(col.biome)
		{
		case BiomeType::Plains:
			s.type = roll < 82 ? StructureType::Village : StructureType::Ruins;
			break;
		case BiomeType::Forest:
			s.type = roll < 55 ? StructureType::Village : StructureType::Ruins;
			break;
		case BiomeType::Taiga:
			s.type = roll < 70 ? StructureType::Village : StructureType::Ruins;
			s.style = 1;
			break;
		case BiomeType::SnowyTaiga:
			s.type = roll < 50 ? StructureType::Village : StructureType::Igloo;
			s.style = 3;
			break;
		case BiomeType::CherryGrove:
			s.type = roll < 60 ? StructureType::Shrine : StructureType::Village;
			s.style = 2;
			break;
		case BiomeType::Desert:
			s.type = roll < 55 ? StructureType::DesertTemple : StructureType::Village;
			s.style = 4;
			break;
		case BiomeType::Beach: s.type = StructureType::Lighthouse; break;
		case BiomeType::Mountains:
			if(col.height < 150)
				s.type = StructureType::Ruins;
			break;
		default: break;
		}
	if(s.type == StructureType::None)
		return none;

	// ── Validation du terrain ───────────────────────────────────
	auto ringStats = [&](int r, int samples, int &dry, int &mn, int &mx) {
		dry = 0;
		mn = 1 << 20;
		mx = -(1 << 20);
		for(int i = 0; i < samples; ++i)
			{
				float a = i * 2.0f * PI / samples;
				int hh = H(cx + static_cast<int>(std::cos(a) * r), cz + static_cast<int>(std::sin(a) * r));
				dry += hh > SEA_LEVEL + 1;
				mn = std::min(mn, hh);
				mx = std::max(mx, hh);
			}
	};
	int dry, mn, mx;
	switch(s.type)
		{
		case StructureType::Village:
			ringStats(20, 12, dry, mn, mx);
			if(dry < 10 || mx - mn > 16)
				return none;
			s.radius = 46;
			break;
		case StructureType::DesertTemple:
			ringStats(11, 8, dry, mn, mx);
			if(dry < 8 || mx - mn > 9)
				return none;
			s.radius = 17;
			break;
		case StructureType::Shrine:
			ringStats(12, 8, dry, mn, mx);
			if(dry < 8 || mx - mn > 8)
				return none;
			s.radius = 30;
			break;
		case StructureType::Lighthouse:
			{
				// Mer à proximité
				int water = 0;
				for(int i = 0; i < 8; ++i)
					{
						float a = i * PI / 4.0f;
						for(int r = 8; r <= 20; r += 6)
							if(H(cx + static_cast<int>(std::cos(a) * r), cz + static_cast<int>(std::sin(a) * r))
							   <= SEA_LEVEL - 1)
								{
									water++;
									break;
								}
					}
				if(water < 2)
					return none;
				s.radius = 8;
				break;
			}
		default: // ruines, igloo : sol pas trop pentu
			ringStats(4, 8, dry, mn, mx);
			if(dry < 8 || mx - mn > 4)
				return none;
			s.radius = s.type == StructureType::Igloo ? 7 : 11;
			break;
		}
	return s;
}

bool StructureGenerator::occupied(int wx, int wz) const
{
	for(int rx = floorDiv(wx - MAX_REACH, REGION); rx <= floorDiv(wx + MAX_REACH, REGION); ++rx)
		for(int rz = floorDiv(wz - MAX_REACH, REGION); rz <= floorDiv(wz + MAX_REACH, REGION); ++rz)
			{
				StructureSite s;
				if(!siteInRegion(rx, rz, s))
					continue;
				int dx = wx - s.center.x, dz = wz - s.center.z;
				if(dx * dx + dz * dz <= s.radius * s.radius)
					return true;
			}
	return false;
}

bool StructureGenerator::findNearest(StructureType type, const glm::ivec2 &from, int maxDist,
									 StructureSite &out) const
{
	int frx = floorDiv(from.x, REGION), frz = floorDiv(from.y, REGION);
	int maxRing = maxDist / REGION + 1;
	float best = 1e30f;
	for(int r = 0; r <= maxRing; ++r)
		{
			if(best < 1e29f && (r - 1) * REGION > std::sqrt(best))
				break;
			for(int rx = frx - r; rx <= frx + r; ++rx)
				for(int rz = frz - r; rz <= frz + r; ++rz)
					{
						if(std::max(std::abs(rx - frx), std::abs(rz - frz)) != r)
							continue;
						StructureSite s;
						if(!siteInRegion(rx, rz, s))
							continue;
						if(type != StructureType::None && s.type != type)
							continue;
						float dx = static_cast<float>(s.center.x - from.x);
						float dz = static_cast<float>(s.center.z - from.y);
						float d2 = dx * dx + dz * dz;
						if(d2 < best && d2 <= static_cast<float>(maxDist) * maxDist)
							{
								best = d2;
								out = s;
							}
					}
		}
	return best < 1e29f;
}

// ── Plans (cache) et pose dans les chunks ───────────────────────
std::shared_ptr<const StructureBlueprint>
StructureGenerator::blueprint(int rx, int rz, const StructureSite &site) const
{
	uint64_t key = regionKey(rx, rz);
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_blueprints.find(key);
		if(it != m_blueprints.end())
			{
				it->second.lastUse = ++m_useCounter;
				return it->second.bp;
			}
	}
	auto bp = std::make_shared<StructureBlueprint>();
	build(site, *bp);

	std::lock_guard<std::mutex> lock(m_mutex);
	// Cache borné : on oublie les plans les moins récemment utilisés
	if(m_blueprints.size() >= 48)
		{
			auto oldest = m_blueprints.begin();
			for(auto it = m_blueprints.begin(); it != m_blueprints.end(); ++it)
				if(it->second.lastUse < oldest->second.lastUse)
					oldest = it;
			m_blueprints.erase(oldest);
		}
	auto &slot = m_blueprints[key];
	if(!slot.bp)
		slot.bp = std::move(bp);
	slot.lastUse = ++m_useCounter;
	return slot.bp;
}

void StructureGenerator::place(Chunk &chunk) const
{
	glm::ivec3 o = chunk.toWorldPos(0, 0, 0);
	for(int rx = floorDiv(o.x - MAX_REACH, REGION); rx <= floorDiv(o.x + CHUNK_SIZE_X + MAX_REACH, REGION); ++rx)
		for(int rz = floorDiv(o.z - MAX_REACH, REGION); rz <= floorDiv(o.z + CHUNK_SIZE_Z + MAX_REACH, REGION); ++rz)
			{
				StructureSite s;
				if(!siteInRegion(rx, rz, s))
					continue;
				if(s.center.x + MAX_REACH < o.x || s.center.x - MAX_REACH >= o.x + CHUNK_SIZE_X
				   || s.center.z + MAX_REACH < o.z || s.center.z - MAX_REACH >= o.z + CHUNK_SIZE_Z)
					continue;
				auto bp = blueprint(rx, rz, s);
				if(bp->max.x < o.x || bp->min.x >= o.x + CHUNK_SIZE_X || bp->max.z < o.z
				   || bp->min.z >= o.z + CHUNK_SIZE_Z)
					continue;
				for(const auto &b : bp->blocks)
					{
						int lx = b.pos.x - o.x, lz = b.pos.z - o.z;
						if(lx < 0 || lx >= CHUNK_SIZE_X || lz < 0 || lz >= CHUNK_SIZE_Z)
							continue;
						BlockType cur = chunk.getBlockUnchecked(lx, b.pos.y, lz);
						const BlockData &cd = BlockDatabase::Get(cur);
						bool soft = cur == BlockType::Air || cd.renderType == RenderType::Cross
									|| (cd.isTransparent && cur != BlockType::Glass);
						switch(b.mode)
							{
							case Mode::Set: chunk.setBlock(lx, b.pos.y, lz, b.type); break;
							case Mode::Fill:
								if(soft || cur == BlockType::Water)
									chunk.setBlock(lx, b.pos.y, lz, b.type);
								break;
							case Mode::Clear: chunk.setBlock(lx, b.pos.y, lz, BlockType::Air); break;
							case Mode::ClearSoft:
								if(soft && cur != BlockType::Water)
									chunk.setBlock(lx, b.pos.y, lz, BlockType::Air);
								break;
							}
					}
			}
}

void StructureGenerator::build(const StructureSite &site, StructureBlueprint &bp) const
{
	switch(site.type)
		{
		case StructureType::Village: buildVillage(site, bp); break;
		case StructureType::DesertTemple: buildDesertTemple(site, bp); break;
		case StructureType::Ruins: buildRuins(site, bp); break;
		case StructureType::Igloo: buildIgloo(site, bp); break;
		case StructureType::Shrine: buildShrine(site, bp); break;
		case StructureType::Lighthouse: buildLighthouse(site, bp); break;
		default: break;
		}
}

// ── Village ─────────────────────────────────────────────────────
void StructureGenerator::buildVillage(const StructureSite &s, StructureBlueprint &bp) const
{
	Rng r(s.seed);
	const Style st = villageStyle(s.style);
	const int cx = s.center.x, cz = s.center.z;
	auto H = [&](int x, int z) { return m_gen.getHeightAt(x, z); };

	struct Plan
	{
		glm::ivec3 origin;
		int rot, w, d;
		bool farm;
		bool chimney;
	};
	std::vector<Plan> plans;
	std::vector<Rect> used = {{cx - 4, cz - 4, cx + 5, cz + 5}}; // puits
	std::vector<glm::ivec2> doors;

	// ── Parcelles : maisons puis champs, porte tournée vers le puits ──
	int houses = r.range(5, 8), farms = r.range(1, 2);
	for(int attempt = 0; attempt < 90 && (houses > 0 || farms > 0); ++attempt)
		{
			bool farm = houses == 0 || (farms > 0 && attempt % 4 == 3);
			int w = farm ? 7 : 5 + 2 * r.range(0, 1);
			int d = farm ? 9 : 5 + 2 * r.range(0, 2);
			float ang = r.f() * 2.0f * PI;
			float dist = (farm ? 22.0f : 11.0f) + r.f() * (farm ? 16.0f : 24.0f);
			int px = cx + static_cast<int>(std::cos(ang) * dist);
			int pz = cz + static_cast<int>(std::sin(ang) * dist);
			int ddx = cx - px, ddz = cz - pz;
			int rot = std::abs(ddx) > std::abs(ddz) ? (ddx > 0 ? 1 : 3) : (ddz > 0 ? 2 : 0);

			glm::ivec2 off = rotate2(w / 2, d / 2, rot);
			glm::ivec3 origin(px - off.x, 0, pz - off.y);
			Builder b{bp, origin, rot};
			glm::ivec3 c0 = b.w(-1, 0, -1), c1 = b.w(w, 0, d);
			Rect rect{std::min(c0.x, c1.x), std::min(c0.z, c1.z), std::max(c0.x, c1.x),
					  std::max(c0.z, c1.z)};
			bool clash = false;
			for(const Rect &u : used)
				clash |= rect.overlaps(u, 2);
			if(clash)
				continue;

			int mn = 1 << 20, mx = -(1 << 20), sum = 0, n = 0;
			for(int x = 0; x < w; ++x)
				for(int z = 0; z < d; ++z)
					{
						glm::ivec3 p = b.w(x, 0, z);
						int hh = H(p.x, p.z);
						mn = std::min(mn, hh);
						mx = std::max(mx, hh);
						sum += hh;
						n++;
					}
			if(mn <= SEA_LEVEL + 1 || mx - mn > (farm ? 2 : 5))
				continue;

			origin.y = farm ? static_cast<int>(std::lround(static_cast<float>(sum) / n)) - 1 : mx - 1;
			plans.push_back({origin, rot, w, d, farm, !farm && r.f() < 0.4f && !st.flatRoof});
			used.push_back(rect);
			if(!farm)
				{
					glm::ivec3 door = Builder{bp, origin, rot}.w(w / 2, 0, -2);
					doors.push_back({door.x, door.z});
					houses--;
				}
			else
				farms--;
		}

	// ── Chemins (posés en premier : les bâtiments ont priorité) ──
	std::set<std::pair<int, int>> path;
	auto addPath = [&](int x, int z) { path.insert({x, z}); };
	for(int dx = -4; dx <= 5; ++dx)
		for(int dz = -4; dz <= 5; ++dz)
			if(dx <= -3 || dx >= 4 || dz <= -3 || dz >= 4)
				addPath(cx + dx, cz + dz); // place autour du puits
	std::vector<std::vector<glm::ivec2>> routes;
	for(const glm::ivec2 &door : doors)
		{
			std::vector<glm::ivec2> route;
			int tx = std::clamp(door.x, cx - 4, cx + 5), tz = std::clamp(door.y, cz - 4, cz + 5);
			int x = door.x, z = door.y;
			bool xFirst = std::abs(door.x - tx) > std::abs(door.y - tz);
			auto walk = [&](int &v, int target, bool alongX) {
				while(v != target)
					{
						v += v < target ? 1 : -1;
						route.push_back({x, z});
						addPath(x, z);
						addPath(alongX ? x : x + 1, alongX ? z + 1 : z);
					}
			};
			addPath(x, z);
			if(xFirst)
				{
					walk(x, tx, true);
					walk(z, tz, false);
				}
			else
				{
					walk(z, tz, false);
					walk(x, tx, true);
				}
			routes.push_back(std::move(route));
		}
	for(auto [x, z] : path)
		{
			int hh = H(x, z);
			if(hh <= SEA_LEVEL)
				continue;
			bp.add({x, hh - 1, z}, BlockType::Dirt_Path, Mode::Set);
			bp.add({x, hh, z}, BlockType::Air, Mode::ClearSoft);
		}

	// ── Bâtiments ───────────────────────────────────────────────
	for(const Plan &p : plans)
		{
			Builder b{bp, p.origin, p.rot};
			const int w = p.w, d = p.d;
			if(p.farm)
				{
					bool pumpkins = r.f() < 0.3f;
					for(int x = 0; x < w; ++x)
						for(int z = 0; z < d; ++z)
							{
								glm::ivec3 wp = b.w(x, 0, z);
								int top = H(wp.x, wp.z) - 1 - p.origin.y; // sol local
								for(int y = 1; y <= std::max(top, 0) + 3; ++y)
									b.clear(x, y, z);
								for(int y = top; y < 0; ++y)
									b.set(x, y, z, BlockType::Dirt);
								bool border = x == 0 || x == w - 1 || z == 0 || z == d - 1;
								if(border)
									b.set(x, 0, z, st.frame);
								else if(x == w / 2)
									b.set(x, 0, z, BlockType::Water);
								else
									{
										b.set(x, 0, z, BlockType::Farmland);
										bool pumpkin = pumpkins && (x + z) % 3 == 0;
										b.set(x, 1, z, pumpkin ? BlockType::Pumpkin : BlockType::Wheat);
									}
							}
					// Bottes de foin au bord du champ
					for(int k = 0; k < 3; ++k)
						{
							glm::ivec3 wp = b.w(w + 1, 0, 1 + k);
							int y = H(wp.x, wp.z) - p.origin.y;
							b.set(w + 1, y, 1 + k, BlockType::Hay_Bale, Mode::Fill);
							if(k == 1)
								b.set(w + 1, y + 1, 1 + k, BlockType::Hay_Bale, Mode::Fill);
						}
					continue;
				}

			const int wallH = 3;
			// Fondations, sol, intérieur
			for(int x = -1; x <= w; ++x)
				for(int z = -1; z <= d; ++z)
					{
						bool inside = x >= 0 && x < w && z >= 0 && z < d;
						glm::ivec3 wp = b.w(x, 0, z);
						int top = H(wp.x, wp.z) - 1 - p.origin.y;
						if(!inside)
							{
								// Pourtour : on dégage la végétation et les reliefs gênants
								for(int y = 1; y <= wallH; ++y)
									b.set(x, y, z, BlockType::Air, y <= top ? Mode::Clear : Mode::ClearSoft);
								continue;
							}
						for(int y = std::min(top, -1); y < 0; ++y)
							b.set(x, y, z, st.foundation);
						bool edge = x == 0 || x == w - 1 || z == 0 || z == d - 1;
						b.set(x, 0, z, edge ? st.foundation : st.floor);
						for(int y = 1; y <= wallH + (w + 1) / 2 + 1; ++y)
							b.clear(x, y, z);
					}
			// Murs et poteaux d'angle
			for(int y = 1; y <= wallH; ++y)
				for(int x = 0; x < w; ++x)
					for(int z = 0; z < d; ++z)
						{
							bool ex = x == 0 || x == w - 1, ez = z == 0 || z == d - 1;
							if(!ex && !ez)
								continue;
							b.set(x, y, z, (ex && ez) ? st.frame : st.wall);
						}
			// Fenêtres
			for(int z = 1; z < d - 1; ++z)
				if(z == d / 2 || (d >= 9 && std::abs(z - d / 2) == 2))
					{
						b.set(0, 2, z, BlockType::Glass);
						b.set(w - 1, 2, z, BlockType::Glass);
					}
			b.set(w / 2, 2, d - 1, BlockType::Glass);
			// Porte (ouverture) et marche
			b.clear(w / 2, 1, 0);
			b.clear(w / 2, 2, 0);
			{
				glm::ivec3 wp = b.w(w / 2, 0, -1);
				int top = H(wp.x, wp.z) - 1 - p.origin.y;
				for(int y = std::min(top, -1); y < 0; ++y)
					b.set(w / 2, y, -1, st.foundation);
				b.set(w / 2, 0, -1, BlockType::Dirt_Path);
			}

			// Toit
			if(st.flatRoof)
				{
					for(int x = -1; x <= w; ++x)
						for(int z = -1; z <= d; ++z)
							b.set(x, wallH + 1, z, st.roof);
					for(int x = 0; x < w; ++x)
						for(int z = 0; z < d; ++z)
							if(x == 0 || x == w - 1 || z == 0 || z == d - 1)
								b.set(x, wallH + 2, z, (x + z) % 2 ? st.frame : st.roof);
				}
			else
				{
					// Toit à deux pans, faîtage dans l'axe de la profondeur
					for(int k = 0;; ++k)
						{
							int left = -1 + k, right = w - k;
							if(left > right)
								break;
							int y = wallH + 1 + k;
							for(int z = -1; z <= d; ++z)
								{
									b.set(left, y, z, st.roof);
									b.set(right, y, z, st.roof);
								}
							// Pignons
							for(int x = left + 1; x < right; ++x)
								{
									b.set(x, y, 0, st.wall);
									b.set(x, y, d - 1, st.wall);
								}
						}
				}
			// Lanterne au plafond, cheminée
			b.set(w / 2, wallH, d / 2, BlockType::Lantern);
			if(p.chimney)
				for(int y = 1; y <= wallH + (w + 1) / 2 + 2; ++y)
					b.set(w - 2, y, d - 2, BlockType::Bricks);
		}

	// ── Puits au centre ─────────────────────────────────────────
	{
		int y0 = H(cx, cz) - 1;
		Builder b{bp, {cx, y0, cz}, 0};
		for(int x = -2; x <= 3; ++x)
			for(int z = -2; z <= 3; ++z)
				{
					int top = H(cx + x, cz + z) - 1 - y0;
					for(int y = 1; y <= std::max(top, 0) + 5; ++y)
						b.clear(x, y, z);
					for(int y = std::min(top, -1); y < 0; ++y)
						b.set(x, y, z, BlockType::Cobblestone);
					b.set(x, 0, z, BlockType::Cobblestone);
				}
		for(int x = -1; x <= 2; ++x)
			for(int z = -1; z <= 2; ++z)
				{
					bool inner = x >= 0 && x <= 1 && z >= 0 && z <= 1;
					b.set(x, -3, z, BlockType::Cobblestone);
					for(int y = -2; y <= 1; ++y)
						b.set(x, y, z, inner ? BlockType::Water : BlockType::Cobblestone);
				}
		for(int x : {-1, 2})
			for(int z : {-1, 2})
				for(int y = 2; y <= 3; ++y)
					b.set(x, y, z, st.frame);
		for(int x = -1; x <= 2; ++x)
			for(int z = -1; z <= 2; ++z)
				b.set(x, 4, z, st.flatRoof ? st.roof : BlockType::Spruce_Planks);
		for(int x = 0; x <= 1; ++x)
			for(int z = 0; z <= 1; ++z)
				b.set(x, 5, z, st.flatRoof ? st.roof : BlockType::Spruce_Planks);
		b.set(0, 6, 0, BlockType::Lantern);
	}

	// ── Lampadaires le long des chemins ─────────────────────────
	std::set<std::pair<int, int>> lamps;
	for(const auto &route : routes)
		for(size_t i = 3; i < route.size(); i += 7)
			{
				glm::ivec2 p = route[i];
				glm::ivec2 dir = i + 1 < route.size() ? route[i + 1] - route[i] : route[i] - route[i - 1];
				glm::ivec2 side(dir.y, -dir.x);
				if(side.x + side.y > 0) // côté opposé à la seconde voie du chemin
					side = -side;
				glm::ivec2 q = p + side;
				bool blocked = path.count({q.x, q.y}) > 0;
				for(const Rect &u : used)
					blocked |= u.contains(q.x, q.y);
				for(auto [lx, lz] : lamps)
					blocked |= std::abs(lx - q.x) + std::abs(lz - q.y) < 5;
				if(blocked)
					continue;
				int hh = H(q.x, q.y);
				if(hh <= SEA_LEVEL)
					continue;
				lamps.insert({q.x, q.y});
				bp.add({q.x, hh, q.y}, st.frame, Mode::Set);
				bp.add({q.x, hh + 1, q.y}, st.frame, Mode::Set);
				bp.add({q.x, hh + 2, q.y}, BlockType::Lantern, Mode::Set);
			}
}

// ── Temple du désert : pyramide à degrés ────────────────────────
void StructureGenerator::buildDesertTemple(const StructureSite &s, StructureBlueprint &bp) const
{
	const int half = 10, levels = 11;
	const int cx = s.center.x, cz = s.center.z;
	int y0 = m_gen.getHeightAt(cx, cz) - 1;
	Builder b{bp, {cx, y0, cz}, static_cast<int>(s.seed & 3u)};
	auto H = [&](int x, int z) {
		glm::ivec3 p = b.w(x, 0, z);
		return m_gen.getHeightAt(p.x, p.z) - 1 - y0;
	};

	// Dégage les dunes au-dessus des gradins, fondations dessous
	for(int x = -half - 1; x <= half + 1; ++x)
		for(int z = -half - 1; z <= half + 1; ++z)
			{
				int step = half - std::max(std::abs(x), std::abs(z));
				int top = H(x, z);
				for(int y = std::max(step, 0) + 1; y <= std::max(top, 0) + 2; ++y)
					b.clear(x, y, z);
				for(int y = std::min(top, -1); y < 0; ++y)
					b.set(x, y, z, BlockType::Sandstone);
			}

	for(int level = 0; level < levels; ++level)
		{
			int sz = half - level;
			for(int x = -sz; x <= sz; ++x)
				for(int z = -sz; z <= sz; ++z)
					{
						int ring = std::max(std::abs(x), std::abs(z));
						bool shell = ring >= sz - 1;
						BlockType t = BlockType::Sandstone;
						if(ring == sz && level == 3)
							t = BlockType::Chiseled_Sandstone;
						else if(ring == sz && level == 6)
							t = BlockType::Vermilion_Planks;
						if(level == 0)
							t = (std::abs(x) <= 1 && std::abs(z) <= 1) ? BlockType::Chiseled_Sandstone
																	   : BlockType::Sandstone;
						else if(level <= 6 && !shell)
							{
								b.clear(x, level, z); // grande salle
								continue;
							}
						b.set(x, level, z, t);
					}
		}
	b.set(0, levels, 0, BlockType::Lantern);

	// Entrée (côté -Z local) et piliers intérieurs
	for(int x = -1; x <= 1; ++x)
		for(int y = 1; y <= 3; ++y)
			for(int z = -half; z <= -half + 2; ++z)
				b.clear(x, y, z);
	for(int px : {-4, 4})
		for(int pz : {-4, 4})
			for(int y = 1; y <= 6; ++y)
				b.set(px, y, pz, BlockType::Chiseled_Sandstone);
	for(int px : {-7, 7})
		for(int pz : {-7, 7})
			b.set(px, 1, pz, BlockType::Torch);
	b.set(0, 6, 0, BlockType::Lantern);

	// Obélisques de part et d'autre de l'entrée
	for(int ox : {-half - 3, half + 3})
		{
			int oz = -half + 1;
			int top = H(ox, oz);
			for(int x = ox - 1; x <= ox + 1; ++x)
				for(int z = oz - 1; z <= oz + 1; ++z)
					{
						int t = H(x, z);
						for(int y = std::min(t, -1); y < 0; ++y)
							b.set(x, y, z, BlockType::Sandstone);
						b.set(x, 0, z, BlockType::Sandstone);
					}
			for(int y = 1; y <= 8 + std::max(top, 0); ++y)
				b.set(ox, y, oz, y % 4 == 0 ? BlockType::Chiseled_Sandstone : BlockType::Sandstone);
			b.set(ox, 9 + std::max(top, 0), oz, BlockType::Lantern);
		}
}

// ── Tour en ruine ───────────────────────────────────────────────
void StructureGenerator::buildRuins(const StructureSite &s, StructureBlueprint &bp) const
{
	Rng r(s.seed);
	const int cx = s.center.x, cz = s.center.z;
	int y0 = m_gen.getHeightAt(cx, cz) - 1;
	Builder b{bp, {cx, y0, cz}, r.range(0, 3)};
	auto H = [&](int x, int z) {
		glm::ivec3 p = b.w(x, 0, z);
		return m_gen.getHeightAt(p.x, p.z) - 1 - y0;
	};
	auto stone = [&]() {
		float v = r.f();
		return v < 0.45f   ? BlockType::Stone_Bricks
			   : v < 0.75f ? BlockType::Mossy_Stone_Bricks
			   : v < 0.92f ? BlockType::Mossy_Cobblestone
						   : BlockType::Cobblestone;
	};

	const float R = 3.0f + r.range(0, 1);
	const int tall = 11 + r.range(0, 7);
	const int ext = static_cast<int>(R) + 1;
	for(int x = -ext; x <= ext; ++x)
		for(int z = -ext; z <= ext; ++z)
			{
				float d = std::sqrt(static_cast<float>(x * x + z * z));
				if(d > R + 0.5f)
					continue;
				int top = H(x, z);
				for(int y = std::min(top, -1); y < 0; ++y)
					b.set(x, y, z, BlockType::Cobblestone);
				b.set(x, 0, z, stone());
				bool wall = d >= R - 0.5f;
				for(int y = 1; y <= tall; ++y)
					{
						if(!wall)
							{
								b.clear(x, y, z);
								continue;
							}
						// Effondrement : de plus en plus de pierres manquent vers le haut
						float decay = std::pow(static_cast<float>(y) / tall, 1.6f) * 0.9f;
						float lean = (x > 0 ? 0.15f : -0.05f);
						if(r.f() < decay + lean * (static_cast<float>(y) / tall))
							continue;
						b.set(x, y, z, stone());
					}
			}
	// Porte, meurtrières
	b.clear(0, 1, -static_cast<int>(R));
	b.clear(0, 2, -static_cast<int>(R));
	b.clear(static_cast<int>(R), 6, 0);
	b.clear(-static_cast<int>(R), 6, 0);
	// Toiles d'araignée
	for(int i = 0; i < 7; ++i)
		{
			int x = r.range(-2, 2), z = r.range(-2, 2);
			if(x * x + z * z >= (R - 0.5f) * (R - 0.5f))
				continue;
			b.set(x, r.range(2, tall - 2), z, BlockType::Cobweb, Mode::Fill);
		}
	b.set(0, 1, 0, BlockType::Torch, Mode::Fill);

	// Pan de mur effondré et éboulis
	int wallLen = 5 + r.range(0, 3);
	for(int k = 1; k <= wallLen; ++k)
		{
			int x = ext + k, top = H(x, 0);
			int hgt = std::max(0, 4 - k / 2 - r.range(0, 1));
			for(int y = std::min(top, -1); y <= top && y < 0; ++y)
				b.set(x, y, 0, BlockType::Cobblestone);
			for(int y = 1; y <= hgt; ++y)
				b.set(x, top + y, 0, stone());
		}
	for(int i = 0; i < 26; ++i)
		{
			float a = r.f() * 2.0f * PI, d = R + 1.5f + r.f() * 5.0f;
			int x = static_cast<int>(std::cos(a) * d), z = static_cast<int>(std::sin(a) * d);
			b.set(x, H(x, z) + 1, z, stone(), Mode::Fill);
		}
}

// ── Igloo ───────────────────────────────────────────────────────
void StructureGenerator::buildIgloo(const StructureSite &s, StructureBlueprint &bp) const
{
	const int cx = s.center.x, cz = s.center.z;
	int y0 = m_gen.getHeightAt(cx, cz) - 1;
	Builder b{bp, {cx, y0, cz}, static_cast<int>(s.seed & 3u)};
	auto H = [&](int x, int z) {
		glm::ivec3 p = b.w(x, 0, z);
		return m_gen.getHeightAt(p.x, p.z) - 1 - y0;
	};
	const float R = 5.4f;
	for(int x = -6; x <= 6; ++x)
		for(int z = -8; z <= 6; ++z)
			{
				int top = H(x, z);
				float rr = std::sqrt(static_cast<float>(x * x + z * z));
				if(rr <= R + 0.5f || (std::abs(x) <= 1 && z >= -8 && z < -4))
					{
						for(int y = std::min(top, -1); y < 0; ++y)
							b.set(x, y, z, BlockType::Snow);
						b.set(x, 0, z, rr < R - 0.8f ? BlockType::Spruce_Planks : BlockType::Snow);
					}
				for(int y = 1; y <= 7; ++y)
					{
						float d = std::sqrt(rr * rr + (y * 1.15f) * (y * 1.15f));
						if(d <= R - 1.0f)
							b.clear(x, y, z);
						else if(d <= R)
							b.set(x, y, z, BlockType::Snow);
					}
			}
	// Tunnel d'entrée, encadrement de sapin, lanternes
	for(int z = -8; z <= -4; ++z)
		for(int y = 1; y <= 3; ++y)
			for(int x = -1; x <= 1; ++x)
				{
					bool hole = x == 0 && y <= 2;
					if(hole)
						b.clear(x, y, z);
					else
						b.set(x, y, z, z == -8 ? BlockType::Spruce_Log : BlockType::Snow);
				}
	b.clear(0, 1, -3);
	b.clear(0, 2, -3);
	b.set(0, 4, -8, BlockType::Lantern);
	b.set(0, 1, 0, BlockType::Lantern);
	b.set(2, 1, 1, BlockType::Hay_Bale);
	b.set(-2, 1, 1, BlockType::Spruce_Planks);
}

// ── Sanctuaire des cerisiers : allée de torii et pavillon ──────
void StructureGenerator::buildShrine(const StructureSite &s, StructureBlueprint &bp) const
{
	const int cx = s.center.x, cz = s.center.z;
	const int rot = static_cast<int>(s.seed & 3u);
	const int w = 9, d = 11;
	glm::ivec2 off = rotate2(w / 2, d / 2, rot);
	glm::ivec3 origin(cx - off.x, 0, cz - off.y);
	auto Hw = [&](const glm::ivec3 &p) { return m_gen.getHeightAt(p.x, p.z); };
	int mx = 0;
	{
		Builder probe{bp, origin, rot};
		for(int x = 0; x < w; ++x)
			for(int z = 0; z < d; ++z)
				mx = std::max(mx, Hw(probe.w(x, 0, z)));
	}
	origin.y = mx; // socle surélevé d'un bloc
	Builder b{bp, origin, rot};
	auto H = [&](int x, int z) { return Hw(b.w(x, 0, z)) - 1 - origin.y; };

	// Allée : chemin de 3 de large vers -Z local, suivant le relief
	const int pathLen = 26;
	for(int z = -pathLen; z < 0; ++z)
		for(int x = w / 2 - 1; x <= w / 2 + 1; ++x)
			{
				int top = H(x, z);
				b.set(x, top, z, BlockType::Dirt_Path);
				b.set(x, top + 1, z, BlockType::Air, Mode::ClearSoft);
			}
	// Lanternes de pierre par paires
	for(int z : {-4, -13, -22})
		for(int x : {w / 2 - 3, w / 2 + 3})
			{
				int top = H(x, z);
				b.set(x, top + 1, z, BlockType::Stone_Bricks);
				b.set(x, top + 2, z, BlockType::Lantern);
			}
	// Torii
	for(int z : {-8, -18})
		{
			int base = std::max(H(w / 2 - 2, z), H(w / 2 + 2, z));
			for(int x : {w / 2 - 2, w / 2 + 2})
				{
					int top = H(x, z);
					for(int y = top + 1; y <= base + 5; ++y)
						b.set(x, y, z, BlockType::Vermilion_Planks);
				}
			for(int x = w / 2 - 1; x <= w / 2 + 1; ++x)
				b.set(x, base + 4, z, BlockType::Spruce_Planks); // nuki
			for(int x = w / 2 - 3; x <= w / 2 + 3; ++x)
				b.set(x, base + 6, z, BlockType::Vermilion_Planks); // kasagi
			b.set(w / 2 - 4, base + 7, z, BlockType::Vermilion_Planks);
			b.set(w / 2 + 4, base + 7, z, BlockType::Vermilion_Planks);
		}

	// Pavillon : socle de pierre, plancher de cerisier, piliers vermillon
	for(int x = -1; x <= w; ++x)
		for(int z = -1; z <= d; ++z)
			{
				int top = H(x, z);
				for(int y = std::min(top, -1); y < 0; ++y)
					b.set(x, y, z, BlockType::Stone_Bricks);
				bool edge = x == -1 || x == w || z == -1 || z == d;
				b.set(x, 0, z, edge ? BlockType::Stone_Bricks : BlockType::Cherry_Planks);
				for(int y = 1; y <= 9; ++y)
					b.clear(x, y, z);
			}
	for(int x : {0, w / 2, w - 1})
		for(int z : {0, d / 2, d - 1})
			{
				if(x == w / 2 && z != d - 1)
					continue;
				for(int y = 1; y <= 4; ++y)
					b.set(x, y, z, BlockType::Vermilion_Planks);
			}
	// Frise sous le toit, murs de plâtre sur les côtés et au fond (façade ouverte)
	for(int y = 5; y <= 6; ++y)
		for(int x = 0; x < w; ++x)
			for(int z = 0; z < d; ++z)
				if(x == 0 || x == w - 1 || z == 0 || z == d - 1)
					b.set(x, y, z, y == 5 ? BlockType::Vermilion_Planks : BlockType::Spruce_Planks);
	for(int y = 1; y <= 4; ++y)
		{
			for(int z = 2; z < d - 1; ++z)
				if(z != d / 2)
					{
						b.set(0, y, z, y == 2 ? BlockType::Glass : BlockType::White_Plaster);
						b.set(w - 1, y, z, y == 2 ? BlockType::Glass : BlockType::White_Plaster);
					}
			for(int x = 1; x < w - 1; ++x)
				if(x != w / 2)
					b.set(x, y, d - 1, BlockType::White_Plaster);
		}
	// Toit à deux niveaux avec avancées
	for(int tier = 0; tier < 2; ++tier)
		{
			int grow = 2 - tier * 2, y = 5 + tier * 3;
			for(int k = 0; k < 3; ++k)
				{
					int x0 = -grow + k + tier, x1 = w - 1 + grow - k - tier;
					int z0 = -grow + k + tier, z1 = d - 1 + grow - k - tier;
					for(int x = x0; x <= x1; ++x)
						for(int z = z0; z <= z1; ++z)
							if(k == 2 || x == x0 || x == x1 || z == z0 || z == z1)
								b.set(x, y + k, z, BlockType::Spruce_Planks);
				}
		}
	for(int z = 2; z <= d - 3; ++z)
		b.set(w / 2, 11, z, BlockType::Vermilion_Planks); // faîtage
	b.set(w / 2, 4, d / 2, BlockType::Lantern);
	b.set(w / 2, 1, d - 2, BlockType::Chiseled_Sandstone); // autel
	b.set(w / 2, 2, d - 2, BlockType::Lantern);
}

// ── Phare ───────────────────────────────────────────────────────
void StructureGenerator::buildLighthouse(const StructureSite &s, StructureBlueprint &bp) const
{
	const int cx = s.center.x, cz = s.center.z;
	int y0 = std::max(m_gen.getHeightAt(cx, cz), SEA_LEVEL + 1) - 1;
	Builder b{bp, {cx, y0, cz}, 0};
	auto H = [&](int x, int z) { return m_gen.getHeightAt(cx + x, cz + z) - 1 - y0; };

	// Socle de pierre (fondations jusqu'au sol ou au fond)
	for(int x = -4; x <= 4; ++x)
		for(int z = -4; z <= 4; ++z)
			{
				if(x * x + z * z > 20)
					continue;
				int top = H(x, z);
				for(int y = std::min(top, -1); y < 0; ++y)
					b.set(x, y, z, BlockType::Stone_Bricks);
				for(int y = 0; y <= 1; ++y)
					b.set(x, y, z, x * x + z * z > 12 && y == 1 ? BlockType::Mossy_Stone_Bricks
																: BlockType::Stone_Bricks);
				for(int y = 2; y <= 30; ++y)
					b.clear(x, y, z);
			}
	// Tour rayée blanc / vermillon
	const int tall = 20;
	for(int y = 2; y < 2 + tall; ++y)
		for(int x = -3; x <= 3; ++x)
			for(int z = -3; z <= 3; ++z)
				{
					float d = std::sqrt(static_cast<float>(x * x + z * z));
					if(d > 2.0f && d <= 3.0f)
						b.set(x, y, z, ((y - 2) / 4) % 2 ? BlockType::Vermilion_Planks : BlockType::White_Plaster);
				}
	b.clear(0, 2, -3);
	b.clear(0, 3, -3);
	for(int y = 6; y < 2 + tall; y += 5)
		b.set(3, y, 0, BlockType::Glass);
	// Galerie, salle de la lanterne, toit conique
	int g = 2 + tall;
	for(int x = -4; x <= 4; ++x)
		for(int z = -4; z <= 4; ++z)
			if(x * x + z * z <= 18)
				b.set(x, g, z, BlockType::Stone_Bricks);
	for(int y = g + 1; y <= g + 3; ++y)
		for(int x = -2; x <= 2; ++x)
			for(int z = -2; z <= 2; ++z)
				{
					float d = std::sqrt(static_cast<float>(x * x + z * z));
					if(d > 1.5f && d <= 2.5f)
						b.set(x, y, z, BlockType::Glass);
				}
	b.set(0, g + 1, 0, BlockType::Lantern);
	b.set(0, g + 2, 0, BlockType::Lantern);
	for(int x = -3; x <= 3; ++x)
		for(int z = -3; z <= 3; ++z)
			{
				int d2 = x * x + z * z;
				if(d2 <= 9)
					b.set(x, g + 4, z, BlockType::Vermilion_Planks);
				if(d2 <= 4)
					b.set(x, g + 5, z, BlockType::Vermilion_Planks);
				if(d2 <= 1)
					b.set(x, g + 6, z, BlockType::Vermilion_Planks);
			}
	b.set(0, g + 7, 0, BlockType::Lantern);
}
