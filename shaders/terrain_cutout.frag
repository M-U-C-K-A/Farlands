#version 450
#extension GL_GOOGLE_include_directive : require
// Terrain à découpe alpha : feuillages, plantes, vitres.
#define CUTOUT
#include "terrain_body.glsl"
