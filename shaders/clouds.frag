#version 450
#extension GL_GOOGLE_include_directive : require
// Nuages volumétriques à mi-résolution : rgb = lumière diffusée,
// a = transmittance. Composés par sky.frag (les nuages sont doux : la
// demi-résolution ne se voit pas, pour un quart du coût).
#define USE_SKY_LUT
#define USE_CLOUDS
#include "atmosphere.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 p = u.invViewProj * vec4(vUV * 2.0 - 1.0, 1.0, 1.0);
    vec3 dir = normalize(p.xyz / p.w);
    if (dir.y <= 0.0 || u.fogParams.w > 0.5)
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float jitter = ign(gl_FragCoord.xy + float(u.misc.w) * 5.588238);
    outColor = marchClouds(u.cameraPos.xyz, dir, 30000.0, 36, 5, jitter,
                           skyRadiance(vec3(0, 1, 0)));
}
