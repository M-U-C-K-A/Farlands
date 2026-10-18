#version 450
#extension GL_GOOGLE_include_directive : require
// Shadow map : même géométrie que le terrain, vue du soleil. Sans vent :
// la shadow map est mise en cache entre les frames.
#include "common.glsl"

layout(location = 0) in uvec4 inPos;  // position locale au chunk (fixe), uv
layout(location = 1) in uint inData;
layout(location = 2) in vec4 inTint;

layout(location = 0) out vec3 vUV;

void main()
{
    VertexData v = unpackVertex(inData);
    vec3 rel = vertexPosition(inPos) + pc.a.xyz;
    gl_Position = u.lightViewProj * vec4(rel, 1.0);
    vUV = vec3(vertexUV(inPos), float(v.layer));
}
