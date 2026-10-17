#version 450
#extension GL_GOOGLE_include_directive : require
// Copie de la scène opaque dans la cible HDR (avant les surfaces d'eau).
#include "common.glsl"

layout(set = 0, binding = B_SCENE_COLOR) uniform sampler2D sceneColor;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(textureLod(sceneColor, vUV, 0.0).rgb, 1.0);
}
