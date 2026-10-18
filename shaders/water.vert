#version 450
#extension GL_GOOGLE_include_directive : require
// Surfaces d'eau (positions relatives à la caméra).
#include "common.glsl"

layout(location = 0) in uvec4 inPos;  // position locale au chunk (fixe), uv
layout(location = 1) in uint inData;
layout(location = 2) in vec4 inTint;

layout(location = 0) out vec3 vRel;
layout(location = 1) flat out uint vNormal;
layout(location = 2) out float vViewDepth;

void main()
{
    VertexData v = unpackVertex(inData);
    vec3 rel = vertexPosition(inPos) + pc.a.xyz;
    gl_Position = u.viewProj * vec4(rel, 1.0);
    vRel = rel;
    vNormal = v.normal;
    vViewDepth = -(u.view * vec4(rel, 1.0)).z;
}
