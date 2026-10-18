#version 450
#extension GL_GOOGLE_include_directive : require
// Feuille / pétale : silhouette procédurale, éclairage translucide, ombres.
#define USE_SKY_LUT
#define USE_CLOUDS
#include "lighting.glsl"

layout(location = 0) in vec3 vRel;
layout(location = 1) in vec2 vLocal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec3 vNormal;
layout(location = 4) in float vAlpha;
layout(location = 5) in float vViewDepth;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

void main()
{
    // Silhouette en amande, pointue aux extrémités ; fondu par tramage
    // Silhouette pixelisée (8x8 texels) pour rester dans le style des blocs
    vec2 q = (floor((vLocal * 0.5 + 0.5) * 8.0) + 0.5) / 8.0 * 2.0 - 1.0;
    float x = q.x, y = q.y;
    float width = 1.0 - y * y;
    if (abs(x) > width || vAlpha < ign(gl_FragCoord.xy)) discard;

    // Nervure centrale légèrement plus sombre
    vec3 albedo = vColor * (abs(x) < 0.08 ? 0.82 : 1.0);

    vec3 N = vNormal;
    vec3 L = u.sunDir.xyz;
    float viewDist = length(vRel);
    vec3 V = -vRel / max(viewDist, 1e-3);
    if (dot(N, V) < 0.0) N = -N;                       // double face

    float shadow = sunShadow(vRel, vec3(0.0, 1.0, 0.0)) * cloudShadow(worldPos(vRel));
    float diffuse = 0.45 + 0.55 * abs(dot(N, L));      // feuille fine : éclairage enveloppé
    float trans = pow(saturate(dot(-V, L)), 3.0) * 0.6; // contre-jour
    vec3 sun = u.sunColor.rgb * (diffuse + trans) * shadow;
    vec3 ambient = skyAmbient(vec3(0.0, 1.0, 0.0));

    vec3 color = albedo * (sun / PI + ambient);
    color = applyFog(color, vRel, viewDist);
    outColor = vec4(color, 1.0);
    outDepth = vViewDepth;
}
