#version 450
#extension GL_GOOGLE_include_directive : require
// Terrain : sommets compacts des chunks, positions relatives à la caméra.
#include "common.glsl"

layout(location = 0) in vec3 inPos;   // locale au chunk
layout(location = 1) in uint inData;
layout(location = 2) in vec4 inTint;

layout(location = 0) out vec3 vRel;
layout(location = 1) out vec3 vUV;    // xy = uv, z = couche
layout(location = 2) flat out uint vNormal;
layout(location = 3) out float vAO;
layout(location = 4) out vec3 vTint;
layout(location = 5) out float vViewDepth;
layout(location = 6) flat out uint vBlock;
layout(location = 7) flat out uint vBlendKind;
layout(location = 8) out float vBlendWeight;   // part du voisin autour du sommet

void main()
{
    VertexData v = unpackVertex(inData);
    vec3 rel = inPos + pc.a.xyz;
    rel += windOffset(rel + u.cameraPos.xyz, v.wave);

    gl_Position = u.viewProj * vec4(rel, 1.0);
    vRel = rel;
    vUV = vec3(v.uv, float(v.layer));
    vNormal = v.normal;
    vAO = float(v.ao);
    vTint = inTint.rgb * 2.0; // teinte encodée sur [0, 2]
    vViewDepth = -(u.view * vec4(rel, 1.0)).z;
    vBlock = v.block;
    vBlendKind = v.blendKind;
    vBlendWeight = float(v.blendCount) * 0.25;
}
