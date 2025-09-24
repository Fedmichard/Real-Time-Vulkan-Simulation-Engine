#version 450

#extension GL_GOOGLE_include_directive : require

#include "opengl.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inFragPos;

layout (location = 0) out vec4 outFragColor;

void main()  {
	// normals
	vec3 norm = normalize(inNormal);

	// sunlight calculations
	float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);
	vec3 sunDir = normalize(-sceneData.sunlightDirection.xyz);

	// diffuse info
	vec3 diffMap = texture(colorTex, inUV).xyz; // diffuse map

	// ambient info
	vec3 ambient = sceneData.sunlightColor.xyz * sceneData.ambientColor.xyz * diffMap;

	// specular info
	vec3 specMap = texture(metalTex, inUV).xyz; // specular map
	vec3 viewDir = normalize(sceneData.cameraPos.xyz - inFragPos);
	
	vec3 lighting = ambient;

	// sunlight
	vec3 diffuse = sceneData.sunlightColor.xyz * max(dot(norm, sunDir), 0.0) * diffMap * sceneData.sunlightColor.w;
	vec3 specular = (sceneData.sunlightColor.xyz) * pow(max(dot(viewDir, reflect(-sunDir, norm)), 0.0), materialData.shininess) * specMap * (materialData.colorFactors.w * sceneData.sunlightColor.w);

	for (int i = 0; i < sceneData.emitterCount; i++) {
		// calcualte light direction
		// this is a vector pointing out of the surface towards the light source
		vec3 lightDir = normalize(sceneData.emitter[i].pos.xyz - inFragPos);

		// calculate distance 
		float distance = length(sceneData.emitter[i].pos.xyz - inFragPos);
		float attenuation = 1.0 / (sceneData.emitter[i].constant + sceneData.emitter[i].linear * distance + sceneData.emitter[i].quadratic * (distance * distance));

		// material specular specStrength
		float materialSpecStrength = materialData.colorFactors.w; // how reflective it is
		float lightSpecStrength = sceneData.emitter[i].color.w; // the intensity of the light

		// diffuse
		// we take the dot product of the light vector and the normal vector (both coming out of the surface)
		// and get a value between 1 and 0.
		// if the dot is 0 that means the rays are orthogonal and the object is hit with darkness
		// if it is 1 you get maximum brightness and the light is facing the exact same direction
		float diff = max(dot(norm, lightDir), 0.0);
		diffuse += sceneData.emitter[i].color.xyz * diff * diffMap * attenuation;

		// specular
		vec3 reflectDir = reflect(-lightDir, norm);
		float spec = pow(max(dot(viewDir, reflectDir), 0.0), materialData.shininess);
		specular += (sceneData.emitter[i].color.xyz) * spec * specMap * (materialSpecStrength * lightSpecStrength) * attenuation;

		// final calculation
		lighting += diffuse + specular;
	}

	outFragColor = vec4(lighting, 1.0f);
}