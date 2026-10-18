#version 450
#extension GL_GOOGLE_include_directive : require
// Brume volumétrique (mi-résolution) : marche le long du rayon de vue jusqu'à
// la surface visible, densité concentrée près du sol et animée, lumière du
// soleil filtrée par la shadow map (rayons de lumière entre les arbres) et
// par les nuages, plus la lumière diffuse du ciel.
// Sortie : rgb = lumière diffusée vers la caméra, a = transmittance.
#define USE_SKY_LUT
#define USE_CLOUDS
#include "lighting.glsl"

layout(set = 0, binding = B_FINAL_DEPTH) uniform sampler2D finalDepth;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

float mistDensity(vec3 world)
{
    float base = u.volumetric.x;
    float h = max(world.y - SEA_LEVEL, 0.0);
    float layer = exp(-h / max(u.volumetric.y, 1.0));
    // Nappes de brume qui dérivent lentement
    vec3 q = world * 0.011 + vec3(u.cameraPos.w * 0.004, 0.0, u.cameraPos.w * 0.0025);
    float n = texture(cloudNoise, q).a;
    return base * (layer * (0.35 + 1.3 * n) + 0.06);
}

float sunVisibility(vec3 rel)
{
    if (u.shadow.w < 0.5) return 1.0;
    vec4 lp = u.lightViewProj * vec4(rel, 1.0);
    vec3 sc = lp.xyz / lp.w;
    if (max(abs(sc.x), abs(sc.y)) >= 1.0 || sc.z <= 0.0 || sc.z >= 1.0) return 1.0;
    return texture(shadowMap, vec3(sc.xy * 0.5 + 0.5, sc.z));
}

void main()
{
    if (u.volumetric.x <= 0.0 || u.fogParams.w > 0.5)
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 p = u.invViewProj * vec4(vUV * 2.0 - 1.0, 1.0, 1.0);
    vec3 dir = normalize(p.xyz / p.w);
    vec3 forward = -vec3(u.view[0][2], u.view[1][2], u.view[2][2]);
    float depth = textureLod(finalDepth, vUV, 0.0).r;
    float tMax = min(depth / max(dot(dir, forward), 0.05), u.volumetric.w);

    const int STEPS = 24;
    float dt = tMax / float(STEPS);
    float t = dt * ign(gl_FragCoord.xy);

    vec3 L = u.sunDir.xyz;
    float mu = dot(dir, L);
    // Diffusion vers l'avant (halo, rayons à contre-jour) + composante isotrope
    float phase = mix(phaseHG(mu, 0.0), phaseHG(mu, 0.78), saturate(u.volumetric.z * 0.35))
                  * (0.6 + 0.4 * u.volumetric.z);
    // Ombre des nuages évaluée une fois par rayon (coût), au point milieu
    float cloudVis = cloudShadow(u.cameraPos.xyz + dir * min(tMax * 0.5, 80.0));
    vec3 sunLight = u.sunColor.rgb * phase * cloudVis * (1.0 - 0.6 * u.grade.w);
    // Lumière du ciel : moyenne zénith / horizon (la brume est éclairée de
    // toutes parts), plus forte sous un ciel couvert où elle domine
    vec3 skyAvg = (skyRadiance(vec3(0.0, 1.0, 0.0)) + skyRadiance(normalize(vec3(dir.x, 0.15, dir.z)))) * 0.5;
    vec3 skyLight = overcast(skyAvg) * (0.8 + 0.6 * u.grade.w);

    vec3 scatter = vec3(0.0);
    float T = 1.0;
    for (int i = 0; i < STEPS; i++)
    {
        vec3 rel = dir * t;
        vec3 world = rel + u.cameraPos.xyz;
        float d = mistDensity(world);
        if (d > 1e-5)
        {
            float vis = sunVisibility(rel);
            vec3 S = (sunLight * vis + skyLight) * d;
            float dT = exp(-d * dt);
            scatter += T * S * (1.0 - dT) / d;
            T *= dT;
            if (T < 0.02) break;
        }
        t += dt;
    }
    outColor = vec4(scatter, T);
}
