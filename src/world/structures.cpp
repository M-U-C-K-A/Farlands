// -----------------------------------------------------------------------------
// Fichier : structures.cpp
// Rôle : Choix des emplacements et plans des structures générées.
// Les bâtiments sont décrits en coordonnées locales (porte côté -Z) puis
// tournés vers leur orientation finale (Builder).
// -----------------------------------------------------------------------------
#include "structures.h"

#include "structure_builder.h"
#include "terrain_generator.h"
#include "village.h"

#include "core/logger.h"

#include <chrono>

#include <algorithm>
#include <cmath>
#include <set>

using Mode = StructureBlueprint::Mode;

using Mode = StructureBlueprint::Mode;
using namespace structgen;

namespace
	{
		constexpr float PI = 3.14159265f;

		uint64_t regionKey(int rx, int rz)
		{
			return (static_cast<uint64_t>(static_cast<uint32_t>(rx)) << 32)
				   | static_cast<uint32_t>(rz);
		}
		// Clés des plans : petites structures et villages séparés
		uint64_t villageKey(int vx, int vz) { return regionKey(vx, vz) ^ 0x8000000080000000ull; }
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
	StructureSite village;
	if(villageNear(cx, cz, 40, village))
		return none; // pas de ruine ni de temple dans un village
	auto H = [&](int x, int z) { return m_gen.getHeightAt(x, z); };
	int roll = static_cast<int>(m_gen.hash(rx, rz, 0x0B1Eu) % 100u);

	StructureSite s;
	s.center = glm::ivec3(cx, col.height, cz);
	s.seed = m_gen.hash(rx, rz, 0xBEEFu);
	switch(col.biome)
		{
		case BiomeType::Plains:
			if(roll < 35)
				s.type = StructureType::Ruins;
			break;
		case BiomeType::Forest:
		case BiomeType::Taiga:
			if(roll < 55)
				s.type = StructureType::Ruins;
			break;
		case BiomeType::SnowyTaiga:
			if(roll < 70)
				s.type = StructureType::Igloo;
			break;
		case BiomeType::CherryGrove:
			if(roll < 75)
				s.type = StructureType::Shrine;
			break;
		case BiomeType::Desert:
			if(roll < 75)
				s.type = StructureType::DesertTemple;
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

// ── Villages (grande grille) ────────────────────────────────────
bool StructureGenerator::villageInRegion(int vx, int vz, StructureSite &out) const
{
	uint64_t key = regionKey(vx, vz);
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_villageSites.find(key);
		if(it != m_villageSites.end())
			{
				out = it->second;
				return out.type != StructureType::None;
			}
	}
	StructureSite site = computeVillageSite(vx, vz);
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_villageSites.emplace(key, site);
	}
	out = site;
	return site.type != StructureType::None;
}

StructureSite StructureGenerator::computeVillageSite(int vx, int vz) const
{
	StructureSite none;
	uint32_t h = m_gen.hash(vx, vz, 0x71A6Eu);
	if(h % 100u >= 80u)
		return none;
	const int margin = VILLAGE_REACH;
	auto H = [&](int x, int z) { return m_gen.getHeightAt(x, z); };
	// Quelques essais dans la région : le premier emplacement assez plat
	for(int attempt = 0; attempt < 6; ++attempt)
		{
			uint32_t r = m_gen.hash(vx * 7 + attempt, vz, 0x5EEDu);
			int cx = vx * VILLAGE_REGION + margin + static_cast<int>(r % (VILLAGE_REGION - 2 * margin));
			int cz = vz * VILLAGE_REGION + margin + static_cast<int>((r >> 12) % (VILLAGE_REGION - 2 * margin));
			if(!m_gen.isAmplifiedWorld() && m_gen.amplifiedMask(cx, cz) > 0.02f)
				continue;
			ColumnInfo col = m_gen.getColumn(cx, cz);
			if(col.height <= SEA_LEVEL + 1)
				continue;
			int style = -1;
			switch(col.biome)
				{
				case BiomeType::Plains:
				case BiomeType::Forest: style = 0; break;
				case BiomeType::Taiga: style = 1; break;
				case BiomeType::CherryGrove: style = 2; break;
				case BiomeType::SnowyTaiga: style = 3; break;
				case BiomeType::Desert: style = 4; break;
				default: break;
				}
			if(style < 0)
				continue;
			// Terrain : sec et assez régulier autour du centre
			int dry = 0, total = 0, mn = 1 << 20, mx = -(1 << 20);
			for(int ring : {14, 30})
				for(int i = 0; i < 12; ++i)
					{
						float a = i * 2.0f * PI / 12.0f;
						int hh = H(cx + static_cast<int>(std::cos(a) * ring), cz + static_cast<int>(std::sin(a) * ring));
						dry += hh > SEA_LEVEL + 1;
						total++;
						mn = std::min(mn, hh);
						mx = std::max(mx, hh);
					}
			if(dry < total - 4 || mx - mn > 18)
				continue;
			StructureSite s;
			s.type = StructureType::Village;
			s.center = glm::ivec3(cx, col.height, cz);
			s.style = style;
			s.seed = m_gen.hash(vx, vz, 0xBEEFu + attempt);
			s.radius = 80 + static_cast<int>(s.seed % 51u); // 80..130
			return s;
		}
	return none;
}

bool StructureGenerator::villageNear(int wx, int wz, int margin, StructureSite &out) const
{
	int reach = VILLAGE_REACH + margin;
	for(int vx = floorDiv(wx - reach, VILLAGE_REGION); vx <= floorDiv(wx + reach, VILLAGE_REGION); ++vx)
		for(int vz = floorDiv(wz - reach, VILLAGE_REGION); vz <= floorDiv(wz + reach, VILLAGE_REGION); ++vz)
			{
				StructureSite s;
				if(!villageInRegion(vx, vz, s))
					continue;
				int dx = wx - s.center.x, dz = wz - s.center.z, r = s.radius + margin;
				if(dx * dx + dz * dz <= r * r)
					{
						out = s;
						return true;
					}
			}
	return false;
}

bool StructureGenerator::occupied(int wx, int wz) const
{
	// Village : masque précis (routes, bâtiments, champs, remparts)
	StructureSite v;
	if(villageNear(wx, wz, 0, v))
		{
			auto bp = blueprint(villageKey(floorDiv(v.center.x, VILLAGE_REGION),
										   floorDiv(v.center.z, VILLAGE_REGION)), v);
			if(bp->blocksTree(wx, wz))
				return true;
		}
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
	if(type == StructureType::Village)
		{
			int fvx = floorDiv(from.x, VILLAGE_REGION), fvz = floorDiv(from.y, VILLAGE_REGION);
			float best = 1e30f;
			int rings = maxDist / VILLAGE_REGION + 1;
			for(int r = 0; r <= rings; ++r)
				{
					if(best < 1e29f && (r - 1) * VILLAGE_REGION > std::sqrt(best))
						break;
					for(int vx = fvx - r; vx <= fvx + r; ++vx)
						for(int vz = fvz - r; vz <= fvz + r; ++vz)
							{
								if(std::max(std::abs(vx - fvx), std::abs(vz - fvz)) != r)
									continue;
								StructureSite s;
								if(!villageInRegion(vx, vz, s))
									continue;
								float dx = static_cast<float>(s.center.x - from.x);
								float dz = static_cast<float>(s.center.z - from.y);
								if(dx * dx + dz * dz < best)
									{
										best = dx * dx + dz * dz;
										out = s;
									}
							}
				}
			return best < 1e29f;
		}
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
StructureGenerator::BlueprintPtr StructureGenerator::blueprint(uint64_t key,
															   const StructureSite &site) const
{
	std::promise<BlueprintPtr> promise;
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		auto it = m_blueprints.find(key);
		if(it != m_blueprints.end())
			{
				it->second.lastUse = ++m_useCounter;
				return it->second.bp;
			}
		auto pending = m_building.find(key);
		if(pending != m_building.end())
			{
				// Un autre thread le construit : on attend son résultat
				auto future = pending->second;
				lock.unlock();
				return future.get();
			}
		m_building.emplace(key, promise.get_future().share());
	}

	auto bp = std::make_shared<StructureBlueprint>();
	auto t0 = std::chrono::steady_clock::now();
	build(site, *bp);
	size_t blocks = bp->blocks.size();
	bp->finalize();
	if(site.type == StructureType::Village)
		{
			double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
			LOG_INFO("Village at (" << site.center.x << ", " << site.center.z << "), radius " << site.radius
									<< ": " << blocks << " blocks planned in " << ms << " ms");
		}

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
	m_blueprints[key] = {bp, ++m_useCounter};
	m_building.erase(key);
	promise.set_value(bp);
	return bp;
}

void StructureGenerator::stamp(Chunk &chunk, const StructureBlueprint &bp) const
{
	glm::ivec2 cp = chunk.getChunkPos();
	auto it = bp.byChunk.find(StructureBlueprint::chunkKey(cp.x, cp.y));
	if(it == bp.byChunk.end())
		return;
	glm::ivec3 o = chunk.toWorldPos(0, 0, 0);
	for(const auto &b : it->second)
		{
			int lx = b.pos.x - o.x, lz = b.pos.z - o.z;
			BlockType cur = chunk.getBlockUnchecked(lx, b.pos.y, lz);
			const BlockData &cd = BlockDatabase::Get(cur);
			bool soft = cur == BlockType::Air || cd.renderType == RenderType::Cross
						|| (cd.isTransparent && cd.renderType == RenderType::Cube && cur != BlockType::Glass);
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

void StructureGenerator::place(Chunk &chunk) const
{
	glm::ivec3 o = chunk.toWorldPos(0, 0, 0);
	// Villages (grande grille)
	for(int vx = floorDiv(o.x - VILLAGE_REACH, VILLAGE_REGION);
		vx <= floorDiv(o.x + CHUNK_SIZE_X + VILLAGE_REACH, VILLAGE_REGION); ++vx)
		for(int vz = floorDiv(o.z - VILLAGE_REACH, VILLAGE_REGION);
			vz <= floorDiv(o.z + CHUNK_SIZE_Z + VILLAGE_REACH, VILLAGE_REGION); ++vz)
			{
				StructureSite s;
				if(!villageInRegion(vx, vz, s))
					continue;
				int reach = s.radius + 24;
				if(s.center.x + reach < o.x || s.center.x - reach >= o.x + CHUNK_SIZE_X
				   || s.center.z + reach < o.z || s.center.z - reach >= o.z + CHUNK_SIZE_Z)
					continue;
				stamp(chunk, *blueprint(villageKey(vx, vz), s));
			}
	// Petites structures
	for(int rx = floorDiv(o.x - MAX_REACH, REGION); rx <= floorDiv(o.x + CHUNK_SIZE_X + MAX_REACH, REGION); ++rx)
		for(int rz = floorDiv(o.z - MAX_REACH, REGION); rz <= floorDiv(o.z + CHUNK_SIZE_Z + MAX_REACH, REGION); ++rz)
			{
				StructureSite s;
				if(!siteInRegion(rx, rz, s))
					continue;
				if(s.center.x + MAX_REACH < o.x || s.center.x - MAX_REACH >= o.x + CHUNK_SIZE_X
				   || s.center.z + MAX_REACH < o.z || s.center.z - MAX_REACH >= o.z + CHUNK_SIZE_Z)
					continue;
				stamp(chunk, *blueprint(regionKey(rx, rz), s));
			}
}

void StructureGenerator::build(const StructureSite &site, StructureBlueprint &bp) const
{
	switch(site.type)
		{
		case StructureType::Village: village::build(m_gen, site, bp); break;
		case StructureType::DesertTemple: buildDesertTemple(site, bp); break;
		case StructureType::Ruins: buildRuins(site, bp); break;
		case StructureType::Igloo: buildIgloo(site, bp); break;
		case StructureType::Shrine: buildShrine(site, bp); break;
		case StructureType::Lighthouse: buildLighthouse(site, bp); break;
		default: break;
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
