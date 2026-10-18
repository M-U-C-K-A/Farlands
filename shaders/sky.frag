#version 450
#extension GL_GOOGLE_include_directive : require
// Ciel plein écran : atmosphère (LUT), disque solaire, nuages volumétriques.
#define USE_SKY_LUT
#define USE_CLOUDS
#include "atmosphere.glsl"

layout(set = 0, binding = B_CLOUDS) uniform sampler2D cloudTex;

layout(location = 0) in vec2 vUV;

float hash13(vec3 p3)
{
    p3 = fract(p3 * 0.1031);
    p3 += dot(p3, p3.zyx + 31.32);
    return fract((p3.x + p3.y) * p3.z);
}

// Étoiles : une sur ~70 cellules d'une grille de directions, scintillantes
vec3 stars(vec3 dir)
{
    vec3 p = dir * 180.0;
    vec3 cell = floor(p);
    float h = hash13(cell);
    if (h < 0.986) return vec3(0.0);
    vec3 off = vec3(hash13(cell + 1.3), hash13(cell + 7.1), hash13(cell + 3.7)) - 0.5;
    float d = length(fract(p) - 0.5 - off * 0.5);
    float mag = pow((h - 0.986) / 0.014, 2.5);
    float twinkle = 0.75 + 0.25 * sin(u.cameraPos.w * (1.5 + h * 6.0) + h * 91.0);
    vec3 tint = mix(vec3(1.0, 0.84, 0.68), vec3(0.72, 0.84, 1.0), hash13(cell + 11.0));
    return tint * smoothstep(0.32, 0.0, d) * (0.15 + mag) * twinkle * 2.2
         * smoothstep(0.0, 0.18, dir.y);
}
layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

void main()
{
    vec4 p = u.invViewProj * vec4(vUV * 2.0 - 1.0, 1.0, 1.0);
    vec3 dir = normalize(p.xyz / p.w);

    // Sous l'eau : pas de ciel visible, seulement la couleur de l'eau
    if (u.fogParams.w > 0.5)
    {
        outColor = vec4(u.waterFog.rgb * u.sunColor.rgb * 0.14, 1.0);
        outDepth = SKY_DEPTH;
        return;
    }

    vec3 color = skyRadiance(dir);

    // Disque solaire (assombri au bord) avec transmittance atmosphérique
    float mu = dot(dir, u.sunDir.xyz);
    bool night = u.shadowTint.w > 0.5;
    if (night)
    {
        color += stars(dir) * (1.0 - u.grade.w);
        // Lune : disque grisé (mers), halo diffus ; carrée avec le soleil carré
        vec3 L = u.sunDir.xyz;
        vec3 t1 = normalize(cross(L, vec3(0.0, 1.0, 0.0)));
        vec3 t2 = cross(t1, L);
        vec2 pl = vec2(dot(dir, t1), dot(dir, t2));
        float R = 0.022;
        bool disc = u.colorFilter.w > 0.5 ? (mu > 0.0 && max(abs(pl.x), abs(pl.y)) < R * 0.9)
                                          : (mu > 0.0 && length(pl) < R);
        if (disc)
        {
            vec2 q = u.colorFilter.w > 0.5 ? floor(pl / R * 8.0) / 8.0 : pl / R;
            float mare = texture(cloudNoise, vec3(q * 0.35 + 0.5, 0.71)).r;
            color += u.sunColor.rgb * 14.0 * mix(0.55, 1.0, smoothstep(0.35, 0.65, mare));
        }
        color += u.sunColor.rgb * 0.4 * pow(saturate(mu), 300.0);
        color += u.sunColor.rgb * 0.05 * pow(saturate(mu), 12.0);
    }
    else if (u.colorFilter.w > 0.5)
    {
        // Soleil carré "à la Minecraft", posé en losange
        vec3 L = u.sunDir.xyz;
        vec3 t1 = normalize(cross(L, vec3(0.0, 1.0, 0.0)));
        vec3 t2 = cross(t1, L);
        vec2 pl = vec2(dot(dir, t1), dot(dir, t2));
        if (mu > 0.0 && abs(pl.x) + abs(pl.y) < 0.045)
            color += u.sunColor.rgb * 60.0;
        color += u.sunColor.rgb * 0.15 * pow(saturate(mu), 64.0);
    }
    else
    {
        float sunAngle = 0.0048;
        float cosR = cos(sunAngle);
        if (mu > cosR && dir.y > -0.02)
        {
            float r = sqrt(saturate((1.0 - mu) / (1.0 - cosR)));
            float limb = 1.0 - 0.6 * (1.0 - sqrt(max(1.0 - r * r, 0.0)));
            color += u.sunColor.rgb * 900.0 * limb;
        }
    }
    // Ciel couvert : le bleu disparaît derrière la couche nuageuse
    color = mix(color, overcast(color), 0.6);

    // Nuages (calculés à mi-résolution par clouds.frag)
    vec4 c = texture(cloudTex, vUV);
    color = color * c.a + c.rgb;

    outColor = vec4(color, 1.0);
    outDepth = SKY_DEPTH;
}
