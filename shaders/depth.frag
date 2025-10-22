#version 450

#extension GL_GOOGLE_include_directive : require

#include "depth_inputs.glsl"

layout (location = 0) out vec4 outFragColor;

float near = 10; // 0.1 originally
float far = 100.0;

float LinearizeDepth(float depth) {
	float z = depth * 2.0 - 1.0;
	return (2.0 * near * far) / (far + near - z * (far - near));
}

void main()  {
	float depth = LinearizeDepth(gl_FragCoord.z) / far;
	outFragColor = vec4(vec3(depth), 1.0f);
}