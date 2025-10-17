#version 450

#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inFragPos;

layout (location = 0) out vec4 outFragColor;

void main()  {
	outFragColor = vec4(vec3(gl_FragCoord.z), 1.0f);
}