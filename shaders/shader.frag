#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in float fragAO;
layout(location = 4) in vec3 fragWorldPos;
layout(location = 5) flat in float fragBlockType;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    float time;
} ubo;

layout(binding = 1) uniform sampler2DArray texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // ── Texture de base ────────────────────────────────────
    vec4 texColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);

    // ── Éclairage directionnel (soleil) ────────────────────
    vec3 lightDir = normalize(vec3(0.4, 1.0, 0.3));  // direction du soleil
    vec3 norm = normalize(fragNormal);
    float diff = max(dot(norm, lightDir), 0.0);

    // Composante ambiante de base (les faces non éclairées ne sont pas noires)
    float ambient = 0.35;
    float lighting = ambient + (1.0 - ambient) * diff;

    // ── Ambient Occlusion ──────────────────────────────────
    // AO va de 0 (totalement occlus) à 1 (pas d'occlusion)
    // On adoucit l'effet pour un rendu plus naturel
    float aoFactor = mix(0.15, 1.0, fragAO);

    // ── Couleur éclairée ───────────────────────────────────
    vec3 litColor = texColor.rgb * lighting * aoFactor;

    // ── Brouillard (fog) ───────────────────────────────────
    float dist = distance(fragWorldPos, ubo.viewPos);
    float fogStart = 60.0;
    float fogEnd = 120.0;
    float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);

    // ── Couleur du ciel / brouillard ─────
    vec3 skyColor = vec3(0.40, 0.65, 1.0);

    vec3 finalColor = mix(litColor, skyColor, fogFactor);

    outColor = vec4(finalColor, texColor.a);
}
