#version 450
#extension GL_GOOGLE_include_directive : require
// Shadow map : découpe alpha (feuillages, plantes) pour des ombres ajourées.
#include "common.glsl"

layout(set = 0, binding = B_BLOCK_ALBEDO) uniform sampler2DArray blockAlbedo;

layout(location = 0) in vec3 vUV;

void main()
{
    if (textureLod(blockAlbedo, vUV, 0.0).a < 0.5) discard;
}
