#include "textures.h"

#include "core/logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../core/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../core/stb_image_resize2.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace
	{
		constexpr int MAX_TILE = 64;

		float srgbToLinear(uint8_t c)
		{
			float v = c / 255.0f;
			return v <= 0.04045f ? v / 12.92f
								 : std::pow((v + 0.055f) / 1.055f, 2.4f);
		}

		/// Normal map (espace tangent) déduite de la luminance de l'albédo
		std::vector<uint8_t> normalFromAlbedo(const std::vector<uint8_t> &rgba,
											  int size, float strength)
		{
			std::vector<float> h(size * size);
			for(int i = 0; i < size * size; ++i)
				{
					const uint8_t *p = &rgba[i * 4];
					h[i] = 0.2126f * srgbToLinear(p[0]) + 0.7152f * srgbToLinear(p[1])
						   + 0.0722f * srgbToLinear(p[2]);
					h[i] = std::sqrt(h[i]); // compresse les fortes valeurs
				}
			auto H = [&](int x, int y) {
				x = (x % size + size) % size;
				y = (y % size + size) % size;
				return h[y * size + x];
			};

			std::vector<uint8_t> out(size * size * 4);
			float px = static_cast<float>(size) / 16.0f; // échelle pixel-art
			for(int y = 0; y < size; ++y)
				for(int x = 0; x < size; ++x)
					{
						// Sobel
						float dx = (H(x + 1, y - 1) + 2 * H(x + 1, y) + H(x + 1, y + 1))
								   - (H(x - 1, y - 1) + 2 * H(x - 1, y) + H(x - 1, y + 1));
						float dy = (H(x - 1, y + 1) + 2 * H(x, y + 1) + H(x + 1, y + 1))
								   - (H(x - 1, y - 1) + 2 * H(x, y - 1) + H(x + 1, y - 1));
						float nx = -dx * strength * px / 4.0f;
						float ny = -dy * strength * px / 4.0f;
						float nz = 1.0f;
						float len = std::sqrt(nx * nx + ny * ny + nz * nz);
						uint8_t *o = &out[(y * size + x) * 4];
						o[0] = static_cast<uint8_t>((nx / len * 0.5f + 0.5f) * 255.0f + 0.5f);
						o[1] = static_cast<uint8_t>((ny / len * 0.5f + 0.5f) * 255.0f + 0.5f);
						o[2] = static_cast<uint8_t>((nz / len * 0.5f + 0.5f) * 255.0f + 0.5f);
						o[3] = 255;
					}
			return out;
		}

		/// Réduction 2x d'une normal map (moyenne puis renormalisation)
		std::vector<uint8_t> downsampleNormal(const std::vector<uint8_t> &src,
											  int size)
		{
			int d = std::max(1, size / 2);
			std::vector<uint8_t> out(d * d * 4);
			for(int y = 0; y < d; ++y)
				for(int x = 0; x < d; ++x)
					{
						float n[3] = {0, 0, 0};
						for(int k = 0; k < 4; ++k)
							{
								int sx = std::min(size - 1, x * 2 + (k & 1));
								int sy = std::min(size - 1, y * 2 + (k >> 1));
								const uint8_t *p = &src[(sy * size + sx) * 4];
								for(int c = 0; c < 3; ++c)
									n[c] += p[c] / 255.0f * 2.0f - 1.0f;
							}
						float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
						if(len < 1e-4f)
							{
								n[0] = n[1] = 0;
								n[2] = len = 1;
							}
						uint8_t *o = &out[(y * d + x) * 4];
						for(int c = 0; c < 3; ++c)
							o[c] = static_cast<uint8_t>((n[c] / len * 0.5f + 0.5f) * 255.0f + 0.5f);
						o[3] = 255;
					}
			return out;
		}

		// ── Bruits périodiques pour les nuages ───────────────────
		struct TileNoise
		{
			uint8_t perm[512];
			explicit TileNoise(uint32_t seed)
			{
				uint8_t p[256];
				for(int i = 0; i < 256; ++i)
					p[i] = static_cast<uint8_t>(i);
				uint32_t s = seed;
				for(int i = 255; i > 0; --i)
					{
						s = s * 1664525u + 1013904223u;
						std::swap(p[i], p[(s >> 8) % (i + 1)]);
					}
				for(int i = 0; i < 512; ++i)
					perm[i] = p[i & 255];
			}

			static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
			static float grad(int h, float x, float y, float z)
			{
				h &= 15;
				float u = h < 8 ? x : y;
				float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
				return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
			}

			// Perlin 3D périodique (période en cellules)
			float perlin(float x, float y, float z, int period) const
			{
				int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y)),
					zi = static_cast<int>(std::floor(z));
				float xf = x - xi, yf = y - yi, zf = z - zi;
				auto w = [period](int v) { return ((v % period) + period) % period; };
				int x0 = w(xi), x1 = w(xi + 1), y0 = w(yi), y1 = w(yi + 1),
					z0 = w(zi), z1 = w(zi + 1);
				auto hsh = [&](int a, int b, int c) {
					return perm[perm[perm[a] + b] + c];
				};
				float u = fade(xf), v = fade(yf), t = fade(zf);
				auto lerp = [](float a, float b, float k) { return a + (b - a) * k; };
				float n000 = grad(hsh(x0, y0, z0), xf, yf, zf);
				float n100 = grad(hsh(x1, y0, z0), xf - 1, yf, zf);
				float n010 = grad(hsh(x0, y1, z0), xf, yf - 1, zf);
				float n110 = grad(hsh(x1, y1, z0), xf - 1, yf - 1, zf);
				float n001 = grad(hsh(x0, y0, z1), xf, yf, zf - 1);
				float n101 = grad(hsh(x1, y0, z1), xf - 1, yf, zf - 1);
				float n011 = grad(hsh(x0, y1, z1), xf, yf - 1, zf - 1);
				float n111 = grad(hsh(x1, y1, z1), xf - 1, yf - 1, zf - 1);
				return lerp(lerp(lerp(n000, n100, u), lerp(n010, n110, u), v),
							lerp(lerp(n001, n101, u), lerp(n011, n111, u), v), t);
			}

			// Worley 3D périodique : 1 - distance au point le plus proche
			float worley(float x, float y, float z, int period) const
			{
				int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y)),
					zi = static_cast<int>(std::floor(z));
				float best = 10.0f;
				for(int dz = -1; dz <= 1; ++dz)
					for(int dy = -1; dy <= 1; ++dy)
						for(int dx = -1; dx <= 1; ++dx)
							{
								int cx = xi + dx, cy = yi + dy, cz = zi + dz;
								int wx = ((cx % period) + period) % period;
								int wy = ((cy % period) + period) % period;
								int wz = ((cz % period) + period) % period;
								int hh = perm[perm[perm[wx] + wy] + wz];
								float fx = cx + perm[hh] / 255.0f;
								float fy = cy + perm[(hh + 71) & 255] / 255.0f;
								float fz = cz + perm[(hh + 151) & 255] / 255.0f;
								float d = (fx - x) * (fx - x) + (fy - y) * (fy - y)
										  + (fz - z) * (fz - z);
								best = std::min(best, d);
							}
				return 1.0f - std::min(1.0f, std::sqrt(best));
			}
		};
	}

namespace textures
	{
		BlockTextures loadBlockTextures(VkContext &ctx,
										const std::vector<std::string> &paths)
		{
			if(paths.empty())
				throw std::runtime_error("No block textures");

			// Taille commune des couches : la plus grande, plafonnée à MAX_TILE
			int tile = 0;
			for(const auto &path : paths)
				{
					int w, h, c;
					if(!path.empty() && stbi_info(path.c_str(), &w, &h, &c))
						tile = std::max(tile, std::max(w, h));
				}
			tile = std::clamp(tile, 16, MAX_TILE);

			const uint32_t layers = static_cast<uint32_t>(paths.size());
			const uint32_t mips = static_cast<uint32_t>(std::floor(std::log2(tile))) + 1;
			const size_t layerBytes = static_cast<size_t>(tile) * tile * 4;

			std::vector<std::vector<uint8_t>> base(layers);
			for(uint32_t i = 0; i < layers; ++i)
				{
					auto &img = base[i];
					img.assign(layerBytes, 0);
					if(paths[i].empty())
						continue;

					int w, h, c;
					stbi_uc *px = stbi_load(paths[i].c_str(), &w, &h, &c, STBI_rgb_alpha);
					if(!px)
						{
							LOG_WARNING("Missing block texture: " << paths[i]);
							for(int y = 0; y < tile; y++)
								for(int x = 0; x < tile; x++)
									{
										bool m = ((x * 2 / tile) + (y * 2 / tile)) % 2 == 0;
										uint8_t *d = &img[(y * tile + x) * 4];
										d[0] = m ? 255 : 0;
										d[1] = 0;
										d[2] = m ? 255 : 0;
										d[3] = 255;
									}
							continue;
						}
					if(w == tile && h == tile)
						std::memcpy(img.data(), px, layerBytes);
					else if(w > tile || h > tile)
						stbir_resize_uint8_srgb(px, w, h, 0, img.data(), tile, tile, 0,
												STBIR_RGBA);
					else
						for(int y = 0; y < tile; y++)
							for(int x = 0; x < tile; x++)
								std::memcpy(&img[(y * tile + x) * 4],
											&px[(((y * h) / tile) * w + (x * w) / tile) * 4], 4);
					stbi_image_free(px);
				}

			// Mipmaps albédo (sRGB) et normales
			std::vector<std::vector<uint8_t>> albedoMips(mips), normalMips(mips);
			std::vector<std::vector<uint8_t>> curA = base, curN(layers);
			for(uint32_t i = 0; i < layers; ++i)
				curN[i] = normalFromAlbedo(base[i], tile, 1.6f);

			for(uint32_t m = 0; m < mips; ++m)
				{
					int dim = std::max(1, tile >> m);
					for(uint32_t i = 0; i < layers; ++i)
						{
							albedoMips[m].insert(albedoMips[m].end(), curA[i].begin(), curA[i].end());
							normalMips[m].insert(normalMips[m].end(), curN[i].begin(), curN[i].end());
						}
					if(m + 1 == mips)
						break;
					int next = std::max(1, dim / 2);
					for(uint32_t i = 0; i < layers; ++i)
						{
							std::vector<uint8_t> a(static_cast<size_t>(next) * next * 4);
							stbir_resize_uint8_srgb(curA[i].data(), dim, dim, 0, a.data(), next,
													next, 0, STBIR_RGBA);
							curA[i] = std::move(a);
							curN[i] = downsampleNormal(curN[i], dim);
						}
				}

			BlockTextures bt;
			ImageDesc d;
			d.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			d.width = d.height = static_cast<uint32_t>(tile);
			d.mipLevels = mips;
			d.layers = layers;
			d.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			d.format = VK_FORMAT_R8G8B8A8_SRGB;
			bt.albedo = vkutil::createImage(ctx, d);
			vkutil::uploadImage(ctx, bt.albedo, albedoMips);
			d.format = VK_FORMAT_R8G8B8A8_UNORM;
			bt.normal = vkutil::createImage(ctx, d);
			vkutil::uploadImage(ctx, bt.normal, normalMips);

			LOG_INFO("Block textures: " << layers << " layers, " << tile << "x" << tile
										<< ", " << mips << " mips (+ normal maps)");
			return bt;
		}

		GpuImage loadTexture2D(VkContext &ctx, const std::string &path)
		{
			int w, h, c;
			stbi_uc *px = stbi_load(path.c_str(), &w, &h, &c, STBI_rgb_alpha);
			if(!px)
				throw std::runtime_error("failed to load texture image: " + path);
			std::vector<std::vector<uint8_t>> data(1);
			data[0].assign(px, px + static_cast<size_t>(w) * h * 4);
			stbi_image_free(px);

			ImageDesc d;
			d.format = VK_FORMAT_R8G8B8A8_SRGB;
			d.width = static_cast<uint32_t>(w);
			d.height = static_cast<uint32_t>(h);
			d.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			GpuImage img = vkutil::createImage(ctx, d);
			vkutil::uploadImage(ctx, img, data);
			return img;
		}

		GpuImage createCloudNoise(VkContext &ctx)
		{
			auto start = std::chrono::steady_clock::now();
			constexpr int N = 64;
			std::vector<uint8_t> level0(static_cast<size_t>(N) * N * N * 4);
			TileNoise noise(1337);

			auto work = [&](int zBegin, int zEnd) {
				for(int z = zBegin; z < zEnd; ++z)
					for(int y = 0; y < N; ++y)
						for(int x = 0; x < N; ++x)
							{
								float fx = static_cast<float>(x) / N;
								float fy = static_cast<float>(y) / N;
								float fz = static_cast<float>(z) / N;

								// Perlin fbm (fréquences 4..32, périodiques)
								float p = 0.0f, amp = 0.5f, norm = 0.0f;
								for(int o = 0, f = 4; o < 4; ++o, f *= 2)
									{
										p += noise.perlin(fx * f, fy * f, fz * f, f) * amp;
										norm += amp;
										amp *= 0.5f;
									}
								p = std::clamp(p / norm * 0.9f + 0.5f, 0.0f, 1.0f);

								auto wfbm = [&](int f0) {
									return noise.worley(fx * f0, fy * f0, fz * f0, f0) * 0.625f
										   + noise.worley(fx * f0 * 2, fy * f0 * 2, fz * f0 * 2, f0 * 2) * 0.25f
										   + noise.worley(fx * f0 * 4, fy * f0 * 4, fz * f0 * 4, f0 * 4) * 0.125f;
								};
								float w1 = wfbm(4);
								float w2 = wfbm(8);

								// Perlin-Worley : remap du Perlin par le Worley
								float pw = std::clamp((p - (w1 - 1.0f)) / (1.0f - (w1 - 1.0f)), 0.0f, 1.0f);
								pw = std::clamp((pw - 0.35f) / 0.65f, 0.0f, 1.0f);

								uint8_t *o = &level0[((static_cast<size_t>(z) * N + y) * N + x) * 4];
								o[0] = static_cast<uint8_t>(pw * 255.0f);
								o[1] = static_cast<uint8_t>(w2 * 255.0f);
								o[2] = static_cast<uint8_t>(w1 * 255.0f);
								o[3] = static_cast<uint8_t>(p * 255.0f);
							}
			};
			unsigned threads = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
			std::vector<std::thread> pool;
			for(unsigned t = 0; t < threads; ++t)
				pool.emplace_back(work, static_cast<int>(t * N / threads),
								  static_cast<int>((t + 1) * N / threads));
			for(auto &t : pool)
				t.join();

			// Mipmaps 3D (moyenne 2x2x2)
			std::vector<std::vector<uint8_t>> mips;
			mips.push_back(std::move(level0));
			for(int dim = N; dim > 1; dim /= 2)
				{
					int d2 = dim / 2;
					const auto &src = mips.back();
					std::vector<uint8_t> dst(static_cast<size_t>(d2) * d2 * d2 * 4);
					for(int z = 0; z < d2; ++z)
						for(int y = 0; y < d2; ++y)
							for(int x = 0; x < d2; ++x)
								for(int c = 0; c < 4; ++c)
									{
										int sum = 0;
										for(int k = 0; k < 8; ++k)
											{
												int sx = x * 2 + (k & 1), sy = y * 2 + ((k >> 1) & 1),
													sz = z * 2 + (k >> 2);
												sum += src[((static_cast<size_t>(sz) * dim + sy) * dim + sx) * 4 + c];
											}
										dst[((static_cast<size_t>(z) * d2 + y) * d2 + x) * 4 + c]
										  = static_cast<uint8_t>(sum / 8);
									}
					mips.push_back(std::move(dst));
				}

			ImageDesc d;
			d.type = VK_IMAGE_TYPE_3D;
			d.viewType = VK_IMAGE_VIEW_TYPE_3D;
			d.format = VK_FORMAT_R8G8B8A8_UNORM;
			d.width = d.height = d.depth = N;
			d.mipLevels = static_cast<uint32_t>(mips.size());
			d.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			GpuImage img = vkutil::createImage(ctx, d);
			vkutil::uploadImage(ctx, img, mips);

			double ms = std::chrono::duration<double, std::milli>(
						  std::chrono::steady_clock::now() - start)
						  .count();
			LOG_INFO("Cloud noise: " << N << "^3, generated in " << ms << " ms");
			return img;
		}
	}
