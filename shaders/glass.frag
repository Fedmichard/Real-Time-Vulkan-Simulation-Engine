#version 450

#extension GL_GOOGLE_include_directive : require

#include "glass_inputs.glsl"

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main()  {
	// texture
	vec4 diffMap = texture(colorTex, inUV); // diffuse map
	if (diffMap.a < 0.1) {
		discard;
	}

	outFragColor = diffMap;
}