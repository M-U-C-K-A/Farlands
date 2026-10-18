#version 450
#extension GL_GOOGLE_include_directive : require
// Terrain lointain : grille de hauteur par tuile (positions relatives caméra).
#include "common.glsl"

layout(location = 0) in vec3 inPos;      // relatif à l'origine de la tuile
layout(location = 1) in vec4 inNormal;   // w = 1 : eau
layout(location = 2) in vec4 inColor;    // sRGB

layout(location = 0) out vec3 vRel;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vColor;
layout(location = 3) out float vWater;
layout(location = 4) out float vViewDepth;

void main()
{
    vec3 rel = inPos + pc.a.xyz;
    gl_Position = u.viewProj * vec4(rel, 1.0);
    vRel = rel;
    vNormal = normalize(inNormal.xyz);
    vColor = pow(inColor.rgb, vec3(2.2));
    vWater = inNormal.w;
    vViewDepth = -(u.view * vec4(rel, 1.0)).z;
}
