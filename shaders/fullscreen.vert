#version 450
// Triangle plein écran (3 sommets, sans buffer). z = 0 : plan lointain en
// reverse-Z (le ciel ne remplit que les pixels vides).
layout(location = 0) out vec2 vUV;

void main()
{
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
