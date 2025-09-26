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
	vec3 lighting = ambient;

	// specular info
	vec3 specMap = texture(metalTex, inUV).xyz; // specular map
	vec3 viewDir = normalize(sceneData.cameraPos.xyz - inFragPos);

	// sunlight diffuse
	float sunDiff = max(dot(norm, sunDir), 0.0);
	vec3 sunDiffuse = sceneData.sunlightColor.xyz * sunDiff * diffMap * sceneData.sunlightColor.w;

	// sunlight specular
	vec3 sunReflectDir = reflect(-sunDir, norm);
	float sunSpec = pow(max(dot(viewDir, sunReflectDir), 0.0), materialData.shininess);
	vec3 sunSpecular = sceneData.sunlightColor.xyz * sunSpec * specMap * materialData.colorFactors.w * sceneData.sunlightColor.w;

	lighting += sunDiffuse + sunSpecular;

	for (int i = 0; i < sceneData.emitterCount; i++) {
		// calcualte light direction
		// this is a vector pointing out of the surface towards the light source
		vec3 lightDir = normalize(sceneData.emitter[i].pos.xyz - inFragPos);

		// calculate distance 
		float distance = length(sceneData.emitter[i].pos.xyz - inFragPos);
		float attenuation = 1.0 / (sceneData.emitter[i].constant + sceneData.emitter[i].linear
			* distance + sceneData.emitter[i].quadratic * (distance * distance));

		// diffuse
		// we take the dot product of the light vector and the normal vector (both coming out of the surface)
		// and get a value between 1 and 0.
		// if the dot is 0 that means the rays are orthogonal and the object is hit with darkness
		// if it is 1 you get maximum brightness and the light is facing the exact same direction
		float pntDiff = max(dot(norm, lightDir), 0.0);
		vec3 pntDiffuse = sceneData.emitter[i].color.xyz * pntDiff * diffMap * sceneData.emitter[i].color.w * attenuation;

		// specular
		vec3 pntReflectDir = reflect(-lightDir, norm);
		float pntSpec = pow(max(dot(viewDir, pntReflectDir), 0.0), materialData.shininess);
		vec3 pntSpecular = sceneData.emitter[i].color.xyz * pntSpec * specMap * materialData.colorFactors.w * sceneData.emitter[i].color.w * attenuation;

		// final calculation
		lighting += pntDiffuse + pntSpecular;
	}

	outFragColor = vec4(lighting, 1.0f);
}