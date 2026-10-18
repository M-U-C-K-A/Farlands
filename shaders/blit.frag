#version 450
#extension GL_GOOGLE_include_directive : require
// Copie de la scène opaque (couleur + profondeur linéaire) dans les cibles de
// la passe d'eau.
#include "common.glsl"

layout(set = 0, binding = B_SCENE_COLOR) uniform sampler2D sceneColor;
layout(set = 0, binding = B_LINEAR_DEPTH) uniform sampler2D linearDepth;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

void main()
{
    outColor = vec4(textureLod(sceneColor, vUV, 0.0).rgb, 1.0);
    outDepth = textureLod(linearDepth, vUV, 0.0).r;
}
