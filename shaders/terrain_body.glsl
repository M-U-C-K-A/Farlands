// -----------------------------------------------------------------------------
// terrain_body.glsl — shader de fragment du terrain (inclus par terrain.frag
// et terrain_cutout.frag). Albédo + normal map, soleil avec ombres douces,
// ciel ambiant, occlusion ambiante, translucidité des feuillages, brume.
// CUTOUT : découpe alpha (feuillages, plantes) ; sinon opaque, sans discard
// pour préserver le test de profondeur anticipé.
// -----------------------------------------------------------------------------
#define USE_SKY_LUT
#define USE_CLOUDS
#include "lighting.glsl"

layout(set = 0, binding = B_BLOCK_ALBEDO) uniform sampler2DArray blockAlbedo;
layout(set = 0, binding = B_BLOCK_NORMAL) uniform sampler2DArray blockNormal;

layout(location = 0) in vec3 vRel;
layout(location = 1) in vec3 vUV;
layout(location = 2) flat in uint vNormal;
layout(location = 3) in float vAO;
layout(location = 4) in vec3 vTint;
layout(location = 5) in float vViewDepth;
layout(location = 6) flat in uint vBlock;
layout(location = 7) flat in uint vBlendKind;
layout(location = 8) in float vBlendWeight;
layout(location = 9) in float vBlockLight;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

bool isLeaves(uint b) { return b == 7u || b == 17u || b == 19u || b == 21u; }
// Lanterne, torche, citrouille-lanterne : parties claires de la texture émissives
bool isEmissive(uint b) { return b == 35u || b == 36u || b == 46u; }

const vec3 BLOCK_LIGHT_COLOR = vec3(1.0, 0.66, 0.36);
const float BLOCK_LIGHT_INTENSITY = 2.6;
const float EMISSIVE_INTENSITY = 7.0;

const uint BLOCK_GRASS = 2u, BLOCK_SAND = 8u, BLOCK_SNOW = 15u;

uint blendBlockId(uint kind)
{
    return kind == 1u ? BLOCK_GRASS : (kind == 2u ? BLOCK_SAND : BLOCK_SNOW);
}

float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Transition douce entre deux surfaces (herbe/sable/neige) : masque en pixel
// art (16 texels par bloc) + bruit à grande échelle. Les deux faces d'une
// frontière utilisent le même champ "A" (type d'id le plus petit), ce qui
// rend le motif continu d'un bloc à l'autre. Sur les flancs d'une marche, le
// poids varie verticalement jusqu'au dessus du voisin en contrebas.
vec3 surfaceBlend(vec3 selfAlbedo, vec3 world)
{
    if (vBlendKind == 0u || vNormal == 3u || vNormal == 6u) return selfAlbedo;
    uint partnerId = blendBlockId(vBlendKind);
    float layer = vBlendKind == 1u ? u.blendLayers.x : (vBlendKind == 2u ? u.blendLayers.y : u.blendLayers.z);
    vec3 partner = texture(blockAlbedo, vec3(vUV.xy, layer)).rgb;
    if (partnerId == BLOCK_GRASS) partner *= vTint;

    // Coordonnées du motif dans le plan de la face (légèrement à l'intérieur
    // du bloc : pas de scintillement sur les arêtes entières)
    vec3 inner = world - faceNormal(vNormal) * 0.001;
    vec2 facePos = vNormal == 2u ? inner.xz
                 : (vNormal <= 1u ? vec2(inner.x, inner.y) : vec2(inner.z, inner.y));
    vec2 texel = floor(facePos * 16.0);
    float fine = hash12(texel + (vNormal == 2u ? vec2(0.0) : vec2(517.0, 0.0)));
    float coarse = texture(cloudNoise, vec3(inner.xz * 0.11, 0.37)).a;
    float n = clamp(0.5 + (fine - 0.5) * 0.45 + (coarse - 0.5) * 1.1, 0.0, 1.0);

    bool selfIsA = vBlock < partnerId;
    float aness = selfIsA ? 1.0 - vBlendWeight : vBlendWeight;
    bool showA = n < aness;
    bool showPartner = selfIsA ? !showA : showA;
    return showPartner ? partner : selfAlbedo;
}

void main()
{
    vec4 albedoSample = texture(blockAlbedo, vUV);
#ifdef CUTOUT
    if (albedoSample.a < 0.5) discard;
#endif
    // La teinte ne s'applique qu'à l'herbe pour les faces en transition
    bool tintSelf = vBlendKind == 0u || vBlock == BLOCK_GRASS;
    vec3 albedo = albedoSample.rgb * (tintSelf ? vTint : vec3(1.0));
    albedo = surfaceBlend(albedo, worldPos(vRel));

    bool plant = vNormal == 6u;
    bool leaves = isLeaves(vBlock);
    vec3 geomN = faceNormal(vNormal);
    vec3 N = geomN;
    if (!plant)
    {
        vec3 T, B;
        faceTangents(vNormal, T, B);
        vec3 nt = texture(blockNormal, vUV).xyz * 2.0 - 1.0;
        nt.xy *= leaves ? 0.5 : 1.0;
        N = normalize(T * nt.x + B * nt.y + geomN * nt.z);
    }

    vec3 L = u.sunDir.xyz;
    vec3 world = worldPos(vRel);
    float viewDist = length(vRel);
    vec3 V = -vRel / max(viewDist, 1e-3);

    // ── Soleil ──────────────────────────────────────────────────
    float NdotL = dot(N, L);
    float geomNdotL = plant ? 1.0 : dot(geomN, L);
    float shadow = geomNdotL > 0.0 ? sunShadow(vRel, plant ? vec3(0, 1, 0) : geomN) : 0.0;
    shadow *= cloudShadow(world);

    float diffuse;
    if (plant)
        diffuse = 0.55 + 0.25 * saturate(L.y);           // feuilles fines : éclairage enveloppé
    else
        diffuse = saturate(NdotL) * saturate(geomNdotL * 4.0);

    vec3 sun = u.sunColor.rgb * diffuse * shadow;

    // Translucidité (lumière qui traverse feuilles et herbes)
    if (leaves || plant)
    {
        float back = saturate(dot(-V, L));
        float trans = pow(back, 4.0) * 0.6 + 0.15;
        float visible = leaves ? shadow : max(shadow, 0.3);
        sun += u.sunColor.rgb * trans * visible * vec3(0.9, 1.0, 0.6) * 0.5;
    }

    // ── Ciel ambiant + occlusion ────────────────────────────────
    float ao = 1.0 - vAO * 0.2;
    ao *= ao;
    vec3 ambient = skyAmbient(plant ? vec3(0, 1, 0) : N) * ao;

    // Fond immergé : la lumière traverse l'eau avant d'atteindre le sol
    // (atténuation colorée selon la profondeur) + caustiques
    float depthBelow = SEA_LEVEL - world.y;
    if (depthBelow > 0.0)
    {
        vec3 sunPath = exp(-WATER_ABSORPTION * depthBelow / max(L.y, 0.2));
        sun *= sunPath * (0.6 + caustics(world));
        ambient *= exp(-WATER_ABSORPTION * depthBelow * 0.7);
    }

    // Lumière des blocs : décroissance exponentielle par niveau (~ Minecraft)
    float level = vBlockLight * 15.0;
    float blockLight = level > 0.5 ? pow(0.80, 15.0 - level) : 0.0;
    vec3 torch = BLOCK_LIGHT_COLOR * blockLight * BLOCK_LIGHT_INTENSITY * mix(1.0, ao, 0.6);

    vec3 color = albedo * (sun / PI + ambient + torch);
    if (isEmissive(vBlock))
    {
        float lum = dot(albedo, vec3(0.2126, 0.7152, 0.0722));
        color += albedo * smoothstep(0.22, 0.65, lum) * EMISSIVE_INTENSITY;
    }

    // Légère brillance de la neige
    if (vBlock == 15u)
    {
        vec3 H = normalize(L + V);
        float spec = pow(saturate(dot(N, H)), 64.0) * 0.12;
        color += u.sunColor.rgb * spec * shadow / PI;
    }

    color = applyFog(color, vRel, viewDist);

    outColor = vec4(color, 1.0);
    outDepth = vViewDepth;
}
