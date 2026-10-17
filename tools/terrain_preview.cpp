// -----------------------------------------------------------------------------
// Fichier : terrain_preview.cpp
// Rôle : Outil de développement — rend une carte vue de dessus (PNG) du
// générateur de terrain : biomes, relief ombré, eau, neige.
// Usage : terrain_preview [sortie.png] [taille] [centreX] [centreZ] [pas]
// -----------------------------------------------------------------------------
#include "world/terrain_generator.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#include "core/stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char **argv)
{
	std::string out = argc > 1 ? argv[1] : "terrain_preview.png";
	int size = argc > 2 ? std::atoi(argv[2]) : 1024;
	int cxw = argc > 3 ? std::atoi(argv[3]) : 0;
	int czw = argc > 4 ? std::atoi(argv[4]) : 0;
	int step = argc > 5 ? std::atoi(argv[5]) : 2;

	TerrainGenerator gen(42);
	std::vector<int> heights(size * size);
	std::vector<uint8_t> biomes(size * size);
	std::vector<uint8_t> img(size * size * 3);

	unsigned n = std::max(1u, std::thread::hardware_concurrency());
	std::vector<std::thread> threads;
	for(unsigned t = 0; t < n; ++t)
		threads.emplace_back([&, t] {
			for(int y = static_cast<int>(t); y < size; y += static_cast<int>(n))
				for(int x = 0; x < size; ++x)
					{
						int wx = cxw + (x - size / 2) * step;
						int wz = czw + (y - size / 2) * step;
						ColumnInfo c = gen.getColumn(wx, wz);
						heights[y * size + x] = c.height;
						biomes[y * size + x] = static_cast<uint8_t>(c.biome);
					}
		});
	for(auto &t : threads)
		t.join();

	int minH = 999, maxH = 0;
	int counts[static_cast<int>(BiomeType::Count)] = {};
	for(int i = 0; i < size * size; ++i)
		{
			minH = std::min(minH, heights[i]);
			maxH = std::max(maxH, heights[i]);
			counts[biomes[i]]++;
		}

	for(int y = 0; y < size; ++y)
		for(int x = 0; x < size; ++x)
			{
				int i = y * size + x;
				int h = heights[i];
				glm::vec3 col = getBiomeData(static_cast<BiomeType>(biomes[i])).color;
				int hx = heights[y * size + std::min(x + 1, size - 1)];
				int hy = heights[std::min(y + 1, size - 1) * size + x];
				float shade = 1.0f + (h - hx) * 0.06f / step + (h - hy) * 0.06f / step;
				shade = std::clamp(shade, 0.45f, 1.4f);
				if(h <= SEA_LEVEL)
					{
						float d = std::clamp((SEA_LEVEL - h) / 20.0f, 0.0f, 1.0f);
						col = glm::mix(glm::vec3(0.25f, 0.55f, 0.75f),
									   glm::vec3(0.05f, 0.15f, 0.45f), d);
						shade = 1.0f;
					}
				col *= shade;
				for(int c = 0; c < 3; ++c)
					img[i * 3 + c] = static_cast<uint8_t>(
					  std::clamp(col[c] * 255.0f, 0.0f, 255.0f));
			}

	stbi_write_png(out.c_str(), size, size, 3, img.data(), size * 3);
	std::printf("height range %d..%d (sea %d)\n", minH, maxH, SEA_LEVEL);
	for(int b = 0; b < static_cast<int>(BiomeType::Count); ++b)
		std::printf("  %-14s %5.1f%%\n",
					getBiomeData(static_cast<BiomeType>(b)).name.c_str(),
					100.0f * counts[b] / (size * size));
	return 0;
}
