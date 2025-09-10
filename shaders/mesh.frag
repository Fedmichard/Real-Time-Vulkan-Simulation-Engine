#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inFragPos;

layout (location = 0) out vec4 outFragColor;

void main()  {
	vec3 norm = normalize(inNormal);
	float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);

	// object color
	vec3 color = inColor * texture(colorTex, inUV).xyz;
	// calculate light direction
	vec3 lightDir = normalize(sceneData.emitter.pos.xyz - inFragPos);

	// calculate distance 
	float distance = length(sceneData.emitter.pos.xyz - inFragPos);
	float attenuation = 5.0 / (distance * distance);

	// calculate ambient
	vec3 ambient = sceneData.sunlightColor.xyz * sceneData.ambientColor.xyz;

	// calculate diffuse
	float diff = max(dot(inNormal, lightDir), 0.0);
	vec3 diffuse = diff * sceneData.emitter.color.xyz * attenuation;

	// calculate specular
	float specStrength = 0.5;
	vec3 viewDir = normalize(sceneData.cameraPos.xyz - inFragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3 specular = spec * sceneData.emitter.color.xyz * specStrength * attenuation;

	outFragColor = vec4((ambient + diffuse + specular) * color, 1.0f);
}