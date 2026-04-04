#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    float time;
    mat4 invView;
    mat4 invProj;
} ubo;

layout(location = 0) in vec3 vViewDir;
layout(location = 0) out vec4 outColor;

float hash(vec3 p) {
    p  = fract( p*0.3183099+.1 );
    p *= 17.0;
    return fract( p.x*p.y*p.z*(p.x+p.y+p.z) );
}

void main() {
    vec3 dir = normalize(vViewDir);
    
    // ── Base Sky Color ──
    vec3 skyColor = vec3(0.40, 0.65, 1.0);
    vec3 color = skyColor;

    // ── Fixed Sun (matches lightDir in shader.frag) ──
    vec3 sunDir = normalize(vec3(0.4, 1.0, 0.3));

    // ── Drawing Sun ──
    float sunDot = dot(dir, sunDir);
    if (sunDot > 0.999) { // Sharp sun disk
        color += vec3(1.0, 0.9, 0.6);
    } else if (sunDot > 0.985) { // Sun aura/bloom
        color += vec3(1.0, 0.8, 0.3) * pow((sunDot - 0.985) / 0.014, 4.0) * 0.5;
    }

    outColor = vec4(color, 1.0);
}
