#version 450
#extension GL_GOOGLE_include_directive : require
// Environnement équirectangulaire (ciel + nuages, qualité réduite) : utilisé
// pour les reflets de l'eau.
#define USE_SKY_LUT
#define USE_CLOUDS
#include "atmosphere.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 dir = envDir(vUV);
    vec3 sky = skyRadiance(dir);
    if (dir.y > 0.0)
    {
        vec3 ro = vec3(u.cameraPos.x, max(u.cameraPos.y, SEA_LEVEL), u.cameraPos.z);
        vec4 c = marchClouds(ro, dir, 30000.0, 14, 3, 0.5, skyRadiance(vec3(0, 1, 0)));
        sky = sky * c.a + c.rgb;
    }
    outColor = vec4(sky, 1.0);
}
