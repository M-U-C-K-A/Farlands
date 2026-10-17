#version 450
#extension GL_GOOGLE_include_directive : require
// LUT du ciel : diffusion atmosphérique pour toutes les directions.
#include "atmosphere.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 dir = skyLutDir(vUV);
    vec3 single = atmosphere(dir, u.sunDir.xyz, 24, 8);
    // La diffusion simple sous-estime fortement le bleu du zénith : terme
    // isotrope approchant la diffusion multiple (s'éteint au crépuscule)
    float day = saturate(u.sunDir.y * 4.0 + 0.15);
    vec3 multi = vec3(0.05, 0.10, 0.20) * day * (0.6 + 0.4 * saturate(dir.y + 0.2));
    outColor = vec4(single * 2.5 + multi, 1.0);
}
