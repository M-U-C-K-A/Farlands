#version 450
#extension GL_GOOGLE_include_directive : require
// Feuilles qui tombent : un quad orienté par instance (6 sommets générés).
#include "common.glsl"

layout(location = 0) in vec4 inRelSize;   // xyz relatif caméra, w taille
layout(location = 1) in vec4 inColor;     // sRGB
layout(location = 2) in vec3 inAngles;    // x rotation, y inclinaison, z alpha

layout(location = 0) out vec3 vRel;
layout(location = 1) out vec2 vLocal;     // [-1, 1]
layout(location = 2) out vec3 vColor;
layout(location = 3) out vec3 vNormal;
layout(location = 4) out float vAlpha;
layout(location = 5) out float vViewDepth;

const vec2 CORNERS[6] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(1, 1),
                               vec2(1, 1), vec2(-1, 1), vec2(-1, -1));

void main()
{
    vec2 c = CORNERS[gl_VertexIndex];
    float a = inAngles.x, t = inAngles.y;
    vec3 X = vec3(cos(a), 0.0, sin(a));
    vec3 side = cross(X, vec3(0.0, 1.0, 0.0));
    vec3 Y = cos(t) * vec3(0.0, 1.0, 0.0) + sin(t) * side;
    vec3 rel = inRelSize.xyz + (X * c.x * 0.6 + Y * c.y) * inRelSize.w * 0.5;

    gl_Position = u.viewProj * vec4(rel, 1.0);
    vRel = rel;
    vLocal = c;
    vColor = pow(inColor.rgb, vec3(2.2));
    vNormal = normalize(cross(X, Y));
    vAlpha = inAngles.z;
    vViewDepth = -(u.view * vec4(rel, 1.0)).z;
}
