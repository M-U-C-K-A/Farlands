#version 450
#extension GL_GOOGLE_include_directive : require
// Bloom : agrandissement avec filtre tente 3x3, ajouté au niveau supérieur.
#include "common.glsl"

layout(set = 1, binding = 0) uniform sampler2D src;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 t = pc.a.xy * pc.a.z;
    vec3 c = texture(src, vUV).rgb * 4.0;
    c += (texture(src, vUV + t * vec2(-1, 0)).rgb + texture(src, vUV + t * vec2(1, 0)).rgb
          + texture(src, vUV + t * vec2(0, -1)).rgb + texture(src, vUV + t * vec2(0, 1)).rgb) * 2.0;
    c += texture(src, vUV + t * vec2(-1, -1)).rgb + texture(src, vUV + t * vec2(1, -1)).rgb
         + texture(src, vUV + t * vec2(-1, 1)).rgb + texture(src, vUV + t * vec2(1, 1)).rgb;
    outColor = vec4(c / 16.0, 1.0);
}
