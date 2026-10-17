#version 450
#extension GL_GOOGLE_include_directive : require
// Bloom : réduction 13 échantillons (Jimenez, "Next Generation Post
// Processing in Call of Duty"). Premier niveau : seuil doux + moyenne de
// Karis contre les pixels isolés trop brillants.
#include "common.glsl"

layout(set = 1, binding = 0) uniform sampler2D src;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

vec3 karis(vec3 c) { return c / (1.0 + luminance(c) * 0.25); }

void main()
{
    vec2 t = pc.a.xy;
    vec3 a = texture(src, vUV + t * vec2(-2, -2)).rgb;
    vec3 b = texture(src, vUV + t * vec2(0, -2)).rgb;
    vec3 c = texture(src, vUV + t * vec2(2, -2)).rgb;
    vec3 d = texture(src, vUV + t * vec2(-2, 0)).rgb;
    vec3 e = texture(src, vUV).rgb;
    vec3 f = texture(src, vUV + t * vec2(2, 0)).rgb;
    vec3 g = texture(src, vUV + t * vec2(-2, 2)).rgb;
    vec3 h = texture(src, vUV + t * vec2(0, 2)).rgb;
    vec3 i = texture(src, vUV + t * vec2(2, 2)).rgb;
    vec3 j = texture(src, vUV + t * vec2(-1, -1)).rgb;
    vec3 k = texture(src, vUV + t * vec2(1, -1)).rgb;
    vec3 l = texture(src, vUV + t * vec2(-1, 1)).rgb;
    vec3 m = texture(src, vUV + t * vec2(1, 1)).rgb;

    vec3 color;
    if (pc.a.z > 0.5)
    {
        // Groupes pondérés avec moyenne de Karis
        vec3 g0 = karis((a + b + d + e) * 0.25) * 0.125;
        vec3 g1 = karis((b + c + e + f) * 0.25) * 0.125;
        vec3 g2 = karis((d + e + g + h) * 0.25) * 0.125;
        vec3 g3 = karis((e + f + h + i) * 0.25) * 0.125;
        vec3 g4 = karis((j + k + l + m) * 0.25) * 0.5;
        color = g0 + g1 + g2 + g3 + g4;
        color = min(color, vec3(4000.0));
    }
    else
    {
        color = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625
                + (j + k + l + m) * 0.125;
    }
    outColor = vec4(max(color, vec3(0.0)), 1.0);
}
