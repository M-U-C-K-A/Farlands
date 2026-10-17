#version 450
#extension GL_GOOGLE_include_directive : require
// Composition finale : bloom, rayons crépusculaires (god rays), exposition,
// tonemapping ACES, saturation, vignettage, dithering.
#include "common.glsl"

layout(set = 0, binding = B_HDR_COLOR) uniform sampler2D hdrColor;
layout(set = 0, binding = B_LINEAR_DEPTH) uniform sampler2D linearDepth;
layout(set = 0, binding = B_BLOOM) uniform sampler2D bloomTex;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

// Approximation ACES (Stephen Hill) : entrée sRGB linéaire
vec3 acesFitted(vec3 color)
{
    const mat3 ACESInput = mat3(0.59719, 0.07600, 0.02840,
                                0.35458, 0.90834, 0.13383,
                                0.04823, 0.01566, 0.83777);
    const mat3 ACESOutput = mat3(1.60475, -0.10208, -0.00327,
                                 -0.53108, 1.10813, -0.07276,
                                 -0.07367, -0.00605, 1.07602);
    color = ACESInput * color;
    vec3 a = color * (color + 0.0245786) - 0.000090537;
    vec3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
    color = a / b;
    return saturate(ACESOutput * color);
}

// Rayons de lumière : flou radial du masque de ciel vers le soleil
vec3 godRays(vec2 uv)
{
    vec4 sunClip = u.viewProj * vec4(u.sunDir.xyz * 1000.0, 1.0);
    if (sunClip.w <= 0.0) return vec3(0.0);
    vec2 sunUV = sunClip.xy / sunClip.w * 0.5 + 0.5;
    float onScreen = 1.0 - smoothstep(0.4, 1.0, length(sunUV - 0.5) * 1.2);
    if (onScreen <= 0.0 || u.fogParams.w > 0.5) return vec3(0.0);

    const int SAMPLES = 40;
    vec2 delta = (sunUV - uv) / float(SAMPLES);
    vec2 p = uv + delta * ign(gl_FragCoord.xy);
    float decay = 1.0, sum = 0.0;
    for (int i = 0; i < SAMPLES; i++)
    {
        float sky = textureLod(linearDepth, p, 0.0).r > SKY_DEPTH * 0.5 ? 1.0 : 0.0;
        float d = length((p - sunUV) * vec2(u.screen.x * u.screen.w, 1.0));
        sum += sky * decay * exp(-d * 3.0);
        decay *= 0.97;
        p += delta;
    }
    return u.sunColor.rgb * sum / float(SAMPLES) * 0.06 * onScreen;
}

void main()
{
    vec3 hdr = texture(hdrColor, vUV).rgb;
    vec3 color = hdr;
    if (u.post.y > 0.0)   // bloom désactivé : sa texture n'est pas écrite
        color = mix(hdr, texture(bloomTex, vUV).rgb, u.post.y);

    if (u.post.z > 0.5)
        color += godRays(vUV);

    // Exposition (calibrée pour un éclairement solaire de 22)
    color *= u.post.x * 0.5;
    color = acesFitted(color);

    // Saturation et léger contraste
    float l = luminance(color);
    color = max(mix(vec3(l), color, u.post.w), 0.0);

    // Vignettage doux
    vec2 q = vUV - 0.5;
    color *= 1.0 - dot(q, q) * 0.35;

    if (u.misc.x < 0.5)
        color = pow(color, vec3(1.0 / 2.2));      // pas de sRGB matériel
    // Dithering (évite les bandes dans le ciel)
    color += (ign(gl_FragCoord.xy) - 0.5) / 255.0;
    outColor = vec4(color, 1.0);
}
