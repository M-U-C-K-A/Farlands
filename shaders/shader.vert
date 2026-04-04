#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    float time;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in float inAO;
layout(location = 5) in float inBlockType;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out float fragAO;
layout(location = 4) out vec3 fragWorldPos;
layout(location = 5) flat out float fragBlockType;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    
    gl_Position = ubo.proj * ubo.view * worldPos;

    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragNormal = normalize(mat3(ubo.model) * inNormal);
    fragAO = inAO;
    fragWorldPos = worldPos.xyz;
    fragBlockType = inBlockType;
}
