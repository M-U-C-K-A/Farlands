// -----------------------------------------------------------------------------
// atmosphere.glsl — diffusion simple de Rayleigh + Mie (modèle de Nishita),
// tables de ciel (LUT) et nuages volumétriques.
// Constantes identiques à src/renderer/atmosphere.cpp.
// -----------------------------------------------------------------------------
#ifndef ATMOSPHERE_GLSL
#define ATMOSPHERE_GLSL

#include "common.glsl"

const float R_PLANET = 6371e3;
const float R_ATMO = 6471e3;
const vec3 BETA_RAYLEIGH = vec3(5.5e-6, 13.0e-6, 22.4e-6);
const float BETA_MIE = 21e-6;
const float H_RAYLEIGH = 8e3;
const float H_MIE = 1.2e3;
const float G_MIE = 0.76;
const float SUN_INTENSITY = 22.0;

// Intersection rayon / sphère centrée à l'origine : (t entrée, t sortie)
vec2 raySphere(vec3 ro, vec3 rd, float r)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - r * r;
    float d = b * b - c;
    if (d < 0.0) return vec2(1e9, -1e9);
    d = sqrt(d);
    return vec2(-b - d, -b + d);
}

float phaseRayleigh(float mu) { return 3.0 / (16.0 * PI) * (1.0 + mu * mu); }

float phaseHG(float mu, float g)
{
    float g2 = g * g;
    return 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + mu * mu))
           / ((2.0 + g2) * pow(1.0 + g2 - 2.0 * g * mu, 1.5));
}

// Radiance du ciel dans la direction rd (sans le disque solaire)
vec3 atmosphere(vec3 rd, vec3 sunDir, int iSteps, int jSteps)
{
    vec3 ro = vec3(0.0, R_PLANET + 200.0, 0.0);
    vec2 p = raySphere(ro, rd, R_ATMO);
    if (p.x > p.y) return vec3(0.0);
    float tMax = p.y;
    // Le sol arrête le rayon
    vec2 g = raySphere(ro, rd, R_PLANET);
    if (g.x > 0.0) tMax = min(tMax, g.x);

    float iStep = tMax / float(iSteps);
    float iTime = 0.0;
    vec3 totalR = vec3(0.0), totalM = vec3(0.0);
    float odR = 0.0, odM = 0.0;
    float mu = dot(rd, sunDir);

    for (int i = 0; i < iSteps; i++)
    {
        vec3 iPos = ro + rd * (iTime + iStep * 0.5);
        float iHeight = length(iPos) - R_PLANET;
        float odStepR = exp(-iHeight / H_RAYLEIGH) * iStep;
        float odStepM = exp(-iHeight / H_MIE) * iStep;
        odR += odStepR;
        odM += odStepM;

        // Profondeur optique vers le soleil
        float jStep = raySphere(iPos, sunDir, R_ATMO).y / float(jSteps);
        float jTime = 0.0, jodR = 0.0, jodM = 0.0;
        for (int j = 0; j < jSteps; j++)
        {
            vec3 jPos = iPos + sunDir * (jTime + jStep * 0.5);
            float jHeight = length(jPos) - R_PLANET;
            jodR += exp(-jHeight / H_RAYLEIGH) * jStep;
            jodM += exp(-jHeight / H_MIE) * jStep;
            jTime += jStep;
        }
        vec3 attn = exp(-(BETA_MIE * (odM + jodM) + BETA_RAYLEIGH * (odR + jodR)));
        totalR += odStepR * attn;
        totalM += odStepM * attn;
        iTime += iStep;
    }
    // sunDir.w : échelle de la source (1 soleil, ~0.03 lune)
    return SUN_INTENSITY * u.sunDir.w * (phaseRayleigh(mu) * BETA_RAYLEIGH * totalR
                            + phaseHG(mu, G_MIE) * BETA_MIE * totalM);
}

// ── LUT du ciel : plus de résolution près de l'horizon ─────────
vec2 skyLutUV(vec3 dir)
{
    float az = atan(dir.x, dir.z);
    float el = asin(clamp(dir.y, -1.0, 1.0));
    float v = el / (0.5 * PI);
    v = 0.5 + 0.5 * sign(v) * sqrt(abs(v));
    return vec2(az / (2.0 * PI) + 0.5, 1.0 - v);
}

vec3 skyLutDir(vec2 uv)
{
    float az = (uv.x - 0.5) * 2.0 * PI;
    float v = (1.0 - uv.y) * 2.0 - 1.0;
    float el = sign(v) * v * v * 0.5 * PI;
    return vec3(cos(el) * sin(az), sin(el), cos(el) * cos(az));
}

// ── Environnement équirectangulaire (ciel + nuages) ───────────
vec2 envUV(vec3 dir)
{
    return vec2(atan(dir.x, dir.z) / (2.0 * PI) + 0.5,
                0.5 - asin(clamp(dir.y, -1.0, 1.0)) / PI);
}

vec3 envDir(vec2 uv)
{
    float az = (uv.x - 0.5) * 2.0 * PI;
    float el = (0.5 - uv.y) * PI;
    return vec3(cos(el) * sin(az), sin(el), cos(el) * cos(az));
}

#ifdef USE_SKY_LUT
layout(set = 0, binding = B_SKY_LUT) uniform sampler2D skyLut;

vec3 skyRadiance(vec3 dir)
{
    if (dir.y >= 0.0)
        return texture(skyLut, skyLutUV(normalize(dir))).rgb;
    // Sous l'horizon : brume lointaine plus sombre et bleutée (relief non
    // chargé vu de haut), raccordée à la couleur de l'horizon
    vec3 horizon = texture(skyLut, skyLutUV(normalize(vec3(dir.x, 0.0, dir.z)))).rgb;
    vec3 zenith = texture(skyLut, skyLutUV(vec3(0.0, 1.0, 0.0))).rgb;
    vec3 haze = mix(horizon, zenith * 1.4, 0.35) * 0.72;
    return mix(horizon, haze, saturate(-dir.y * 6.0));
}

// Radiance moyenne reçue du ciel par une surface de normale n (divisée par
// PI : multipliée par l'albédo, elle donne directement la radiance diffuse).
// Quelques échantillons de la LUT répartis dans l'hémisphère visible.
vec3 skyAmbient(vec3 n)
{
    vec3 c = normalize(n + vec3(0.0, 0.9, 0.0));
    vec3 t1 = normalize(cross(c, abs(c.y) < 0.95 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    vec3 t2 = cross(c, t1);
    vec3 sky = skyRadiance(c) * 2.0;
    sky += skyRadiance(normalize(c + t1 * 1.2));
    sky += skyRadiance(normalize(c - t1 * 1.2));
    sky += skyRadiance(normalize(c + t2 * 1.2));
    sky += skyRadiance(normalize(c - t2 * 1.2));
    sky /= 6.0;
    float skyVis = 0.62 + 0.38 * n.y;
    // Rebond du sol éclairé par le soleil (albédo moyen ~ 0.25)
    vec3 ground = u.sunColor.rgb * max(u.sunDir.y, 0.0) * (0.25 / PI) * (1.0 - 0.7 * u.grade.w);
    return overcast(sky * skyVis + ground * (0.5 - 0.5 * n.y)) * 1.25;
}
#endif

// ── Nuages volumétriques ───────────────────────────────────────
#ifdef USE_CLOUDS
layout(set = 0, binding = B_CLOUD_NOISE) uniform sampler3D cloudNoise;

const float CLOUD_BOTTOM = 340.0;
const float CLOUD_TOP = 560.0;

vec3 cloudWind() { return vec3(1.0, 0.0, 0.35) * u.cameraPos.w * u.clouds.z; }

float cloudDensity(vec3 p, bool detail)
{
    float h = (p.y - CLOUD_BOTTOM) / (CLOUD_TOP - CLOUD_BOTTOM);
    if (h <= 0.0 || h >= 1.0) return 0.0;

    vec3 q = p + cloudWind();
    vec4 n = texture(cloudNoise, q * 0.0008);
    // Couverture variable à grande échelle (zones dégagées / nuageuses)
    float cover = u.clouds.x + (texture(cloudNoise, q * 0.00009).a - 0.5) * 0.45;
    // Profil vertical : base plate, sommets arrondis (cumulus)
    float profile = smoothstep(0.0, 0.07, h) * smoothstep(1.0, 0.3, h);
    float base = mix(n.r, n.r * n.b, 0.3) * profile;
    float d = saturate((base - (1.0 - cover)) / max(cover, 0.05));
    if (detail && d > 0.0)
    {
        // Érosion des bords par un bruit de Worley fin (aspect "chou-fleur")
        float e = texture(cloudNoise, q * 0.0047 + vec3(0.0, u.cameraPos.w * 0.003, 0.0)).g;
        d = saturate(d - (1.0 - e) * 0.42 * (1.0 - d * 0.7));
    }
    return d;
}

// Ombre des nuages au sol (une seule lecture, le long du soleil)
float cloudShadow(vec3 world)
{
    if (u.clouds.w < 0.5) return 1.0;
    vec3 L = u.sunDir.xyz;
    float t = (0.5 * (CLOUD_BOTTOM + CLOUD_TOP) - world.y) / max(L.y, 0.08);
    float d = cloudDensity(world + L * t, false);
    // Ciel couvert : les nuages bloquent bien plus le soleil
    return mix(1.0, mix(0.35, 0.08, u.grade.w), saturate(d * 2.5));
}

// Marche dans la couche de nuages : rgb = lumière diffusée, a = transmittance
vec4 marchClouds(vec3 ro, vec3 rd, float maxDist, int steps, int lightSteps,
                 float jitter, vec3 ambient)
{
    if (u.clouds.w < 0.5 || abs(rd.y) < 1e-4) return vec4(0.0, 0.0, 0.0, 1.0);
    float t0 = (CLOUD_BOTTOM - ro.y) / rd.y;
    float t1 = (CLOUD_TOP - ro.y) / rd.y;
    if (t0 > t1) { float tmp = t0; t0 = t1; t1 = tmp; }
    t0 = max(t0, 0.0);
    t1 = min(t1, maxDist);
    if (t1 <= t0) return vec4(0.0, 0.0, 0.0, 1.0);
    t1 = min(t1, t0 + 4000.0);

    vec3 L = u.sunDir.xyz;
    float mu = dot(rd, L);
    // Fonction de phase double (pointe avant + rétrodiffusion), par stéradian
    float phase = mix(phaseHG(mu, 0.7), phaseHG(mu, -0.25), 0.35);
    float sigma = u.clouds.y;

    float dt = (t1 - t0) / float(steps);
    float t = t0 + dt * jitter;
    vec3 scatter = vec3(0.0);
    float T = 1.0;
    for (int i = 0; i < steps; i++)
    {
        vec3 p = ro + rd * t;
        float d = cloudDensity(p, true);
        if (d > 0.002)
        {
            // Profondeur optique vers le soleil
            float ld = 0.0;
            float ls = 18.0;
            for (int j = 0; j < lightSteps; j++)
            {
                ld += cloudDensity(p + L * ls * (float(j) + 0.5), false) * ls;
                ls *= 1.6;
            }
            float beer = exp(-ld * sigma * 0.9);
            float powder = 1.0 - exp(-d * sigma * 40.0);
            // 2.5 : approximation de la diffusion multiple
            vec3 sunL = u.sunColor.rgb * 2.5 * beer * mix(1.0, powder, 0.55) * phase;
            float h = (p.y - CLOUD_BOTTOM) / (CLOUD_TOP - CLOUD_BOTTOM);
            vec3 amb = ambient * mix(0.55, 1.1, h);
            vec3 S = (sunL + amb) * d * sigma * u.grade.z;
            float dT = exp(-d * sigma * dt);
            scatter += T * S * (1.0 - dT) / max(d * sigma, 1e-5);
            T *= dT;
            if (T < 0.01) break;
        }
        t += dt;
    }
    // Perspective aérienne : les nuages lointains se fondent dans le ciel
    float fade = exp(-max(t0, 0.0) * 0.00018);
    return vec4(scatter * fade, mix(1.0, T, fade));
}
#endif

#endif
