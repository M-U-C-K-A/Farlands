// -----------------------------------------------------------------------------
// lighting.glsl — ombres douces du soleil et perspective aérienne (brume).
// Nécessite USE_SKY_LUT (atmosphere.glsl) pour la couleur de la brume.
// -----------------------------------------------------------------------------
#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#include "atmosphere.glsl"

layout(set = 0, binding = B_SHADOW_MAP) uniform sampler2DShadow shadowMap;

const vec2 POISSON[12] = vec2[](
    vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696, 0.457),
    vec2(-0.203, 0.621), vec2(0.962, -0.195), vec2(0.473, -0.480),
    vec2(0.519, 0.767), vec2(0.185, -0.893), vec2(0.507, 0.064),
    vec2(0.896, 0.412), vec2(-0.322, -0.933), vec2(-0.792, -0.598));

// Visibilité du soleil (1 = éclairé) avec PCF sur disque de Poisson tourné
float sunShadow(vec3 rel, vec3 geomNormal)
{
    if (u.shadow.w < 0.5) return 1.0;
    float NdotL = dot(geomNormal, u.sunDir.xyz);
    // Normal offset : décale le point le long de la normale (anti acné)
    vec3 p = rel + geomNormal * u.shadow.z * (1.0 + 1.5 * (1.0 - saturate(NdotL)));
    vec4 lp = u.lightViewProj * vec4(p, 1.0);
    vec3 sc = lp.xyz / lp.w;
    vec2 uv = sc.xy * 0.5 + 0.5;
    float edge = max(abs(sc.x), abs(sc.y));
    if (edge >= 1.0 || sc.z <= 0.0 || sc.z >= 1.0) return 1.0;

    float angle = ign(gl_FragCoord.xy) * 2.0 * PI;
    mat2 rot = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
    float radius = 1.6 * u.shadow.x;
    float sum = 0.0;
    for (int i = 0; i < 12; i++)
        sum += texture(shadowMap, vec3(uv + rot * POISSON[i] * radius, sc.z));
    float vis = sum / 12.0;
    // Fondu vers "éclairé" au bord de la zone couverte
    return mix(vis, 1.0, smoothstep(0.85, 1.0, edge));
}

// Brume atmosphérique : distance + densité décroissante avec l'altitude.
// Retourne la couleur finale.
vec3 applyFog(vec3 color, vec3 rel, float viewDist)
{
    vec3 V = rel / max(viewDist, 1e-3);

    // Sous l'eau : absorption rapide vers la couleur de l'eau
    if (u.fogParams.w > 0.5)
    {
        float f = 1.0 - exp(-viewDist * u.waterFog.a);
        return mix(color, u.waterFog.rgb * u.sunColor.rgb * 0.05, f);
    }

    // Brume exponentielle en hauteur (intégrale analytique le long du rayon)
    float density = u.fogParams.x;
    float k = u.fogParams.y;
    float camH = max(u.cameraPos.y - SEA_LEVEL, 0.0);
    float rayY = V.y * viewDist;
    float heightTerm = exp(-camH * k)
                       * ((abs(rayY) > 0.01) ? (1.0 - exp(-rayY * k)) / (rayY * k) : 1.0);
    float fog = 1.0 - exp(-viewDist * density * heightTerm);

    // Fondu complet avant la limite des chunks chargés
    float edge = smoothstep(u.fogParams.z * 0.72, u.fogParams.z * 0.97, viewDist);
    fog = max(fog, edge);

    // Même couleur que le ciel dans cette direction (raccord invisible au
    // bord du monde chargé, y compris vu d'en haut)
    vec3 fogColor = skyRadiance(V);
    // Halo du soleil dans la brume
    float mu = saturate(dot(V, u.sunDir.xyz));
    fogColor += u.sunColor.rgb * 0.012 * pow(mu, 12.0) * (1.0 - edge);
    return mix(color, fogColor, fog);
}

#endif
