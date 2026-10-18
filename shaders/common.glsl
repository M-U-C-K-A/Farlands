// -----------------------------------------------------------------------------
// common.glsl — uniformes par frame, push constants, bindings et utilitaires.
// Le layout de FrameUBO doit correspondre à FrameUniforms (src/core/types.h).
// Toutes les positions envoyées au GPU sont relatives à la caméra.
// -----------------------------------------------------------------------------
#ifndef COMMON_GLSL
#define COMMON_GLSL

layout(set = 0, binding = 0) uniform FrameUBO
{
    mat4 view;          // rotation seule (rendu relatif à la caméra)
    mat4 proj;          // reverse-Z, Y inversé
    mat4 viewProj;
    mat4 invView;
    mat4 invProj;
    mat4 invViewProj;
    mat4 lightViewProj;
    vec4 cameraPos;     // xyz monde, w = temps
    vec4 sunDir;        // xyz vers le soleil
    vec4 sunColor;      // rgb éclairement direct
    vec4 fogParams;     // x densité, y décroissance altitude, z distance de rendu, w sous l'eau
    vec4 waterFog;      // rgb couleur, a densité
    vec4 screen;        // xy taille, zw 1/taille
    vec4 post;          // x exposition, y bloom, z god rays, w saturation
    vec4 clouds;        // x couverture, y densité, z vitesse, w activé
    vec4 shadow;        // x 1/res, y distance, z normal offset, w activé
    vec4 misc;          // x sRGB matériel, y near (<0 : SSR coupé), z far, w frame
    vec4 blendLayers;   // couches de texture des transitions : x herbe, y sable, z neige
    vec4 volumetric;    // x densité de la brume, y hauteur, z rayons, w distance max
    vec4 grade;         // x contraste, y vignettage, z luminosité nuages, w ciel couvert
    vec4 colorFilter;   // rgb filtre, w soleil carré
    vec4 shadowTint;    // rgb teinte des tons sombres
} u;

layout(push_constant) uniform Push
{
    vec4 a;
    vec4 b;
} pc;

// Bindings du set 0 (voir Renderer)
#define B_BLOCK_ALBEDO 1
#define B_BLOCK_NORMAL 2
#define B_SHADOW_MAP   3
#define B_SKY_LUT      4
#define B_SKY_ENV      5
#define B_CLOUD_NOISE  6
#define B_SCENE_COLOR  7
#define B_LINEAR_DEPTH 8
#define B_HDR_COLOR    9
#define B_BLOOM        10
#define B_CLOUDS       11
#define B_FINAL_DEPTH  12
#define B_HDR_RAW      13
#define B_VOLUMETRIC   14

const float PI = 3.14159265359;
const float SKY_DEPTH = 1.0e6;   // profondeur linéaire écrite par le ciel
const float SEA_LEVEL = 62.0;

float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec3 saturate(vec3 x) { return clamp(x, 0.0, 1.0); }

// Bruit à gradient entrelacé (Jimenez) : dithering des marches de rayon
float ign(vec2 p)
{
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

float luminance(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

vec3 worldPos(vec3 rel) { return rel + u.cameraPos.xyz; }

// Ciel couvert : la lumière diffuse et la brume virent au gris (un ciel
// couvert reste lumineux : la luminance est conservée)
vec3 overcast(vec3 c)
{
    vec3 grey = vec3(luminance(c)) * vec3(0.95, 0.97, 1.0);
    return mix(c, grey, u.grade.w);
}

// ── Décodage du Vertex compact (voir src/core/types.h) ─────────
struct VertexData
{
    uint layer;
    uint normal;  // 0 +Z, 1 -Z, 2 +Y, 3 -Y, 4 +X, 5 -X, 6 plante
    uint ao;
    vec2 uv;
    uint block;
    uint wave;
    uint blendKind;   // 0 aucun, 1 herbe, 2 sable, 3 neige
    uint blendCount;  // part du type mélangé autour du coin, en quarts (0..4)
};

VertexData unpackVertex(uint d)
{
    VertexData v;
    v.layer = d & 0x3FFu;
    v.normal = (d >> 10) & 7u;
    v.ao = (d >> 13) & 3u;
    v.uv = vec2(float((d >> 15) & 1u), float((d >> 16) & 1u));
    v.block = (d >> 17) & 0xFFu;
    v.wave = (d >> 25) & 3u;
    v.blendKind = (d >> 27) & 3u;
    v.blendCount = (d >> 29) & 7u;
    return v;
}

vec3 faceNormal(uint n)
{
    if (n == 0u) return vec3(0, 0, 1);
    if (n == 1u) return vec3(0, 0, -1);
    if (n == 2u) return vec3(0, 1, 0);
    if (n == 3u) return vec3(0, -1, 0);
    if (n == 4u) return vec3(1, 0, 0);
    if (n == 5u) return vec3(-1, 0, 0);
    return vec3(0, 1, 0);
}

// Repère tangent (direction de u croissant, de v croissant) par face
void faceTangents(uint n, out vec3 T, out vec3 B)
{
    if (n == 0u)      { T = vec3(1, 0, 0);  B = vec3(0, -1, 0); }
    else if (n == 1u) { T = vec3(-1, 0, 0); B = vec3(0, -1, 0); }
    else if (n == 2u) { T = vec3(1, 0, 0);  B = vec3(0, 0, 1); }
    else if (n == 3u) { T = vec3(1, 0, 0);  B = vec3(0, 0, -1); }
    else if (n == 4u) { T = vec3(0, 0, -1); B = vec3(0, -1, 0); }
    else              { T = vec3(0, 0, 1);  B = vec3(0, -1, 0); }
}

// Ondulation au vent (feuillages : wave 1, haut des plantes : wave 2)
vec3 windOffset(vec3 world, uint wave)
{
    if (wave == 0u) return vec3(0.0);
    float t = u.cameraPos.w;
    float phase = world.x * 0.35 + world.z * 0.27;
    float gust = 0.6 + 0.4 * sin(t * 0.45 + world.x * 0.02 + world.z * 0.015);
    vec3 o = vec3(sin(t * 1.9 + phase), sin(t * 2.7 + phase * 1.3) * 0.4,
                  cos(t * 1.6 + phase * 0.8));
    float amp = (wave == 1u) ? 0.045 : 0.11;
    return o * amp * gust;
}

#endif
