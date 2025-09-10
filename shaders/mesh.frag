#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inFragPos;

layout (location = 0) out vec4 outFragColor;

void main()  {
	float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);

	// object color
	vec3 color = inColor * texture(colorTex,inUV).xyz;

	// calculate light
	vec3 lightDir = normalize(sceneData.emitter.pos.xyz - inFragPos);

	// calculate ambient
	vec3 ambient = sceneData.sunlightColor.xyz * sceneData.ambientColor.xyz;

	// calculate diffuse
	float diff = max(dot(inNormal, lightDir), 0.0);

	// calculate distance 
	float distance = length(sceneData.emitter.pos.xyz - inFragPos);
	float attenuation = 5.0 / (distance * distance);
	vec3 diffuse = diff * sceneData.emitter.color.xyz * attenuation;

	outFragColor = vec4((ambient + diffuse) * color, 1.0f);
}