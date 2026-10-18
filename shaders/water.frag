#version 450
#extension GL_GOOGLE_include_directive : require
// Eau : vagues procédurales, réflexions écran (SSR) avec repli sur
// l'environnement (ciel + nuages), réfraction, absorption de Beer-Lambert,
// diffusion dans l'eau, reflet spéculaire du soleil, vue depuis le dessous.
#define USE_SKY_LUT
#define USE_CLOUDS
#include "lighting.glsl"

layout(set = 0, binding = B_SKY_ENV) uniform sampler2D skyEnv;
layout(set = 0, binding = B_SCENE_COLOR) uniform sampler2D sceneColor;
layout(set = 0, binding = B_LINEAR_DEPTH) uniform sampler2D linearDepth;

layout(location = 0) in vec3 vRel;
layout(location = 1) flat in uint vNormal;
layout(location = 2) in float vViewDepth;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;   // profondeur finale (brume volumétrique)

// Somme de vagues directionnelles : retourne la normale (espace monde)
vec3 waveNormal(vec2 p, float t, float strength)
{
    const vec4 W[6] = vec4[](
        // direction (xy), longueur d'onde, amplitude relative
        vec4(1.0, 0.3, 9.0, 1.0), vec4(-0.6, 1.0, 5.5, 0.7),
        vec4(0.2, -1.0, 3.3, 0.5), vec4(-1.0, -0.4, 1.9, 0.35),
        vec4(0.7, 0.8, 1.1, 0.22), vec4(-0.3, 0.9, 0.65, 0.15));
    vec2 grad = vec2(0.0);
    for (int i = 0; i < 6; i++)
    {
        vec2 d = normalize(W[i].xy);
        float k = 2.0 * PI / W[i].z;
        float c = sqrt(9.81 / k);               // vitesse de phase
        float ph = k * (dot(d, p) - c * t * 0.35);
        float a = W[i].w * 0.09 / k;            // pente max ~ 0.09 : eau calme
        grad += d * (a * k * cos(ph));
    }
    return normalize(vec3(-grad.x * strength, 1.0, -grad.y * strength));
}

vec3 envRadiance(vec3 dir)
{
    dir.y = max(dir.y, 0.0);
    return texture(skyEnv, envUV(normalize(dir))).rgb;
}

// Réflexions en espace écran (marche en espace vue + affinage dichotomique)
bool traceSSR(vec3 originVS, vec3 dirVS, out vec2 hitUV)
{
    float stepLen = 0.5;
    vec3 p = originVS;
    float jitter = ign(gl_FragCoord.xy);
    p += dirVS * stepLen * jitter;
    for (int i = 0; i < 40; i++)
    {
        p += dirVS * stepLen;
        stepLen *= 1.1;
        vec4 clip = u.proj * vec4(p, 1.0);
        if (clip.w <= 0.0) return false;
        vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
        if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return false;
        float sceneD = textureLod(linearDepth, uv, 0.0).r;
        float rayD = -p.z;
        if (rayD > sceneD && sceneD < SKY_DEPTH * 0.5)
        {
            if (rayD - sceneD > stepLen * 3.0 + 1.0) return false; // derrière un objet
            // Affinage
            vec3 a = p - dirVS * stepLen, b = p;
            for (int j = 0; j < 6; j++)
            {
                vec3 m = (a + b) * 0.5;
                vec4 mc = u.proj * vec4(m, 1.0);
                vec2 muv = mc.xy / mc.w * 0.5 + 0.5;
                if (-m.z > textureLod(linearDepth, muv, 0.0).r) b = m; else a = m;
            }
            vec4 hc = u.proj * vec4(b, 1.0);
            hitUV = hc.xy / hc.w * 0.5 + 0.5;
            return true;
        }
    }
    return false;
}

void main()
{
    vec3 world = worldPos(vRel);
    float viewDist = length(vRel);
    vec3 V = -vRel / max(viewDist, 1e-3);
    vec3 L = u.sunDir.xyz;
    float t = u.cameraPos.w;
    bool top = vNormal == 2u;

    // Vagues plus calmes au loin (évite le crénelage)
    float strength = mix(1.0, 0.25, saturate(viewDist / 160.0));
    vec3 N = top ? waveNormal(world.xz, t, strength) : faceNormal(vNormal);
    vec2 screenUV = gl_FragCoord.xy * u.screen.zw;

    // ── Vue depuis le dessous de la surface ─────────────────────
    if (!gl_FrontFacing || u.fogParams.w > 0.5)
    {
        vec3 Nd = -N;
        vec3 Rt = refract(-V, Nd, 1.33);
        vec3 deep = u.waterFog.rgb * u.sunColor.rgb * 0.14;
        vec3 color = (dot(Rt, Rt) > 0.0) ? envRadiance(Rt) * 0.8 : deep;
        color = mix(color, deep, 1.0 - exp(-viewDist * u.waterFog.a));
        outColor = vec4(color, 1.0);
        outDepth = vViewDepth;
        return;
    }

    // ── Réfraction + absorption ────────────────────────────────
    float sceneD = textureLod(linearDepth, screenUV, 0.0).r;
    float thickness = max(sceneD - vViewDepth, 0.0);
    vec2 offset = N.xz * 0.06 * saturate(thickness / 4.0) / max(1.0, vViewDepth * 0.05);
    vec2 refrUV = screenUV + offset;
    float refrD = textureLod(linearDepth, refrUV, 0.0).r;
    if (refrD < vViewDepth) { refrUV = screenUV; refrD = sceneD; }
    vec3 refracted = textureLod(sceneColor, refrUV, 0.0).rgb;
    float pathLen = min(max(refrD - vViewDepth, 0.0), 80.0);

    // Absorption (rouge absorbé en premier) et diffusion turquoise
    vec3 trans = exp(-WATER_ABSORPTION * pathLen);
    float shadow = sunShadow(vRel, vec3(0, 1, 0)) * cloudShadow(world);
    vec3 scatterColor = vec3(0.03, 0.16, 0.17);
    vec3 inscatter = scatterColor * (u.sunColor.rgb * (0.04 + 0.06 * shadow) * saturate(L.y)
                                     + skyRadiance(vec3(0, 1, 0)) * 0.35);
    vec3 under = refracted * trans + inscatter * (1.0 - trans);

    // ── Réflexion ───────────────────────────────────────────────
    vec3 R = reflect(-V, N);
    if (R.y < 0.02) R = normalize(vec3(R.x, 0.02, R.z));
    vec3 reflection = envRadiance(R);
    if (u.misc.y > 0.0)
    {
        vec3 posVS = (u.view * vec4(vRel, 1.0)).xyz;
        vec3 dirVS = normalize(mat3(u.view) * R);
        vec2 hitUV;
        if (traceSSR(posVS, dirVS, hitUV))
        {
            vec2 e = smoothstep(vec2(0.0), vec2(0.08), hitUV)
                     * smoothstep(vec2(1.0), vec2(0.92), hitUV);
            reflection = mix(reflection, textureLod(sceneColor, hitUV, 0.0).rgb, e.x * e.y);
        }
    }

    // Fresnel (Schlick, eau F0 = 0.02)
    float NdotV = saturate(dot(N, V));
    float F = 0.02 + 0.98 * pow(1.0 - NdotV, 5.0);
    vec3 color = mix(under, reflection, F);

    // Reflet du soleil (Blinn-Phong normalisé)
    vec3 H = normalize(L + V);
    float shin = 1200.0;
    float spec = pow(saturate(dot(N, H)), shin) * (shin + 8.0) / (8.0 * PI);
    color += u.sunColor.rgb * spec * F * shadow * saturate(L.y * 4.0);

    color = applyFog(color, vRel, viewDist);
    outColor = vec4(color, 1.0);
    outDepth = vViewDepth;
}
