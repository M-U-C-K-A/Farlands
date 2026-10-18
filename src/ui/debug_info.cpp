#include "debug_info.h"

#include <algorithm>

void DebugInfo::pushFrame(float ms)
{
	frameMs[historyHead] = ms;
	historyHead = (historyHead + 1) % HISTORY;
	if(ms > 0.0f)
		fps = fps * 0.95f + (1000.0f / ms) * 0.05f;

	// Statistiques sur l'historique (300 valeurs : coût négligeable)
	std::array<float, HISTORY> sorted = frameMs;
	float sum = 0.0f;
	int count = 0;
	frameMaxMs = 0.0f;
	for(float v : frameMs)
		if(v > 0.0f)
			{
				sum += v;
				count++;
				frameMaxMs = std::max(frameMaxMs, v);
			}
	frameAvgMs = count ? sum / count : 0.0f;
	std::sort(sorted.begin(), sorted.end());
	float p99 = sorted[static_cast<size_t>(HISTORY * 0.99f)];
	low1Fps = p99 > 0.0f ? 1000.0f / p99 : 0.0f;
}
