#version 450
#extension GL_GOOGLE_include_directive : require
// Applique la brume volumétrique (mi-résolution) à l'image HDR, avec un
// suréchantillonnage guidé par la profondeur (pas de halo sur les contours).
#include "common.glsl"

layout(set = 0, binding = B_HDR_RAW) uniform sampler2D hdrRaw;
layout(set = 0, binding = B_FINAL_DEPTH) uniform sampler2D finalDepth;
layout(set = 0, binding = B_VOLUMETRIC) uniform sampler2D volumetric;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 color = textureLod(hdrRaw, vUV, 0.0).rgb;
    if (u.volumetric.x <= 0.0 || u.fogParams.w > 0.5)
    {
        outColor = vec4(color, 1.0);
        return;
    }

    float d0 = textureLod(finalDepth, vUV, 0.0).r;
    vec2 halfSize = vec2(textureSize(volumetric, 0));
    vec2 st = vUV * halfSize - 0.5;
    vec2 base = floor(st);
    vec2 f = st - base;

    vec4 sum = vec4(0.0);
    float wsum = 0.0;
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++)
        {
            vec2 uv = (base + vec2(i, j) + 0.5) / halfSize;
            float bil = (i == 0 ? 1.0 - f.x : f.x) * (j == 0 ? 1.0 - f.y : f.y);
            float d = textureLod(finalDepth, uv, 0.0).r;
            float w = bil / (1.0 + abs(d - d0) / (0.04 * min(d0, 2000.0) + 0.5)) + 1e-4;
            sum += textureLod(volumetric, uv, 0.0) * w;
            wsum += w;
        }
    vec4 fog = sum / wsum;
    outColor = vec4(color * fog.a + fog.rgb, 1.0);
}
