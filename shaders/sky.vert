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

layout(location = 0) out vec3 vViewDir;

void main() {
    // Generate full-screen triangle using gl_VertexIndex (0, 1, 2)
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec2 posNDC = pos * 2.0f - 1.0f;
    
    // Z = 1.0 targets the far depth plane in Vulkan
    gl_Position = vec4(posNDC, 1.0, 1.0);

    // Compute ray direction corresponding to this fragment
    vec4 target = ubo.invProj * vec4(posNDC.x, posNDC.y, 1.0, 1.0);
    vViewDir = (ubo.invView * vec4(normalize(target.xyz / target.w), 0.0)).xyz;
}
