#version 450
#extension GL_GOOGLE_include_directive : require
// Terrain lointain : éclairage simplifié (soleil, ombre des nuages, ciel),
// eau avec reflet du ciel, brume atmosphérique. Masqué près de la caméra où
// les vrais chunks prennent le relais.
#define USE_SKY_LUT
#define USE_CLOUDS
#include "lighting.glsl"

layout(set = 0, binding = B_SKY_ENV) uniform sampler2D skyEnv;

layout(location = 0) in vec3 vRel;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in float vWater;
layout(location = 4) in float vViewDepth;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

void main()
{
    if (length(vRel.xz) < pc.a.w) discard;

    vec3 world = worldPos(vRel);
    float viewDist = length(vRel);
    vec3 V = -vRel / max(viewDist, 1e-3);
    vec3 L = u.sunDir.xyz;
    vec3 N = normalize(vNormal);
    float clouds = cloudShadow(world);
    vec3 color;

    if (vWater > 0.5)
    {
        // Eau lointaine : Fresnel entre fond sombre et reflet du ciel
        vec3 R = reflect(-V, vec3(0.0, 1.0, 0.0));
        R.y = max(R.y, 0.02);
        vec3 refl = texture(skyEnv, envUV(normalize(R))).rgb;
        float F = 0.02 + 0.98 * pow(1.0 - saturate(V.y), 5.0);
        vec3 deep = vec3(0.02, 0.08, 0.09) * skyAmbient(vec3(0.0, 1.0, 0.0));
        color = mix(deep, refl, F);
        vec3 H = normalize(L + V);
        color += u.sunColor.rgb * pow(saturate(H.y), 800.0) * 20.0 * F * clouds;
    }
    else
    {
        float NdotL = saturate(dot(N, L));
        vec3 sun = u.sunColor.rgb * NdotL * clouds;
        color = vColor * (sun / PI + skyAmbient(N) * 0.85);
    }

    color = applyFog(color, vRel, viewDist);
    outColor = vec4(color, 1.0);
    outDepth = vViewDepth;
}
