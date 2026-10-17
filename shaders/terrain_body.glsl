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

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

bool isLeaves(uint b) { return b == 7u || b == 17u || b == 19u || b == 21u; }

void main()
{
    vec4 albedoSample = texture(blockAlbedo, vUV);
#ifdef CUTOUT
    if (albedoSample.a < 0.5) discard;
#endif
    vec3 albedo = albedoSample.rgb * vTint;

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

    vec3 color = albedo * (sun / PI + ambient);

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
