#version 450
#extension GL_GOOGLE_include_directive : require
// Ciel plein écran : atmosphère (LUT), disque solaire, nuages volumétriques.
#define USE_SKY_LUT
#include "atmosphere.glsl"

layout(set = 0, binding = B_CLOUDS) uniform sampler2D cloudTex;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

void main()
{
    vec4 p = u.invViewProj * vec4(vUV * 2.0 - 1.0, 1.0, 1.0);
    vec3 dir = normalize(p.xyz / p.w);

    // Sous l'eau : pas de ciel visible, seulement la couleur de l'eau
    if (u.fogParams.w > 0.5)
    {
        outColor = vec4(u.waterFog.rgb * u.sunColor.rgb * 0.14, 1.0);
        outDepth = SKY_DEPTH;
        return;
    }

    vec3 color = skyRadiance(dir);

    // Disque solaire (assombri au bord) avec transmittance atmosphérique
    float mu = dot(dir, u.sunDir.xyz);
    float sunAngle = 0.0048;
    float cosR = cos(sunAngle);
    if (mu > cosR && dir.y > -0.02)
    {
        float r = sqrt(saturate((1.0 - mu) / (1.0 - cosR)));
        float limb = 1.0 - 0.6 * (1.0 - sqrt(max(1.0 - r * r, 0.0)));
        color += u.sunColor.rgb * 900.0 * limb;
    }

    // Nuages (calculés à mi-résolution par clouds.frag)
    vec4 c = texture(cloudTex, vUV);
    color = color * c.a + c.rgb;

    outColor = vec4(color, 1.0);
    outDepth = SKY_DEPTH;
}
