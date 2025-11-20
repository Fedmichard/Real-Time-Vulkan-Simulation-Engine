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
	vec4 diffMap = texture(colorTex, inUV); // diffuse map
	// if a fragments alpha value is less than 0.1 discard it (don't render)
	// this is great but not really useful for rendering semi transparent objects
	if (diffMap.a < 0.1) {
		discard;
	}

	// ambient info
	vec3 ambient = sceneData.sunlightColor.xyz * sceneData.ambientColor.xyz * diffMap.xyz;
	vec3 lighting = ambient;

	// specular info
	vec3 specMap = texture(metalTex, inUV).xyz; // specular map
	vec3 viewDir = normalize(sceneData.cameraPos.xyz - inFragPos);

	// sunlight diffuse
	float sunDiff = max(dot(norm, sunDir), 0.0);
	vec3 sunDiffuse = sceneData.sunlightColor.xyz * sunDiff * diffMap.xyz * sceneData.sunlightColor.w;

	// sunlight specular
	vec3 sunReflectDir = reflect(-sunDir, norm);
	float sunSpec = pow(max(dot(viewDir, sunReflectDir), 0.0), materialData.shininess);
	vec3 sunSpecular = sceneData.sunlightColor.xyz * sunSpec * specMap * materialData.colorFactors.w * sceneData.sunlightColor.w;

	lighting += sunDiffuse + sunSpecular;

	for (int i = 0; i < sceneData.emitterCount; i++) {
		// if spotlight
		if (sceneData.emitter[i].pos.w == 1) {
			// light direction
			vec3 lightDir = normalize(sceneData.emitter[i].pos.xyz - inFragPos);

			// calculate distance 
			float distance = length(sceneData.emitter[i].pos.xyz - inFragPos);
			float attenuation = 1.0 / (sceneData.emitter[i].constant + sceneData.emitter[i].linear
				* distance + sceneData.emitter[i].quadratic * (distance * distance));

			float theta = dot(lightDir, normalize(-sceneData.emitter[i].direction.xyz));
			float epsilon = sceneData.emitter[i].cutOff - sceneData.emitter[i].outerCutOff;
			float intensity = clamp((theta - sceneData.emitter[i].outerCutOff) / epsilon, 0.0, 1.0);

			if (theta > sceneData.emitter[i].outerCutOff) {
				// diffuse
				float pntDiff = max(dot(norm, lightDir), 0.0);
				vec3 pntDiffuse = sceneData.emitter[i].color.xyz * pntDiff * diffMap.xyz * sceneData.emitter[i].color.w * attenuation;
				
				vec3 pntReflectDir = reflect(-lightDir, norm);
				float pntSpec = pow(max(dot(viewDir, pntReflectDir), 0.0), 16.0f);
				vec3 pntSpecular = sceneData.emitter[i].color.xyz * pntSpec * specMap * materialData.colorFactors.w * sceneData.emitter[i].color.w * attenuation;

				pntDiffuse *=  intensity;
				pntSpecular *=  intensity;

				lighting += pntDiffuse + pntSpecular;
			}
			
		// if point light
		} else if (sceneData.emitter[i].pos.w == 0) {
			// light direction
			vec3 lightDir = normalize(sceneData.emitter[i].pos.xyz - inFragPos);

			// calculate distance 
			float distance = length(sceneData.emitter[i].pos.xyz - inFragPos);
			float attenuation = 1.0 / (sceneData.emitter[i].constant + sceneData.emitter[i].linear
				* distance + sceneData.emitter[i].quadratic * (distance * distance));

			// diffuse
			float pntDiff = max(dot(norm, lightDir), 0.0);
			vec3 pntDiffuse = sceneData.emitter[i].color.xyz * pntDiff * diffMap.xyz * sceneData.emitter[i].color.w * attenuation;

			// specular
			vec3 pntReflectDir = reflect(-lightDir, norm);
			float pntSpec = pow(max(dot(viewDir, pntReflectDir), 0.0), materialData.shininess);
			vec3 pntSpecular = sceneData.emitter[i].color.xyz * pntSpec * specMap * materialData.colorFactors.w * sceneData.emitter[i].color.w * attenuation;

			// final calculation
			lighting += pntDiffuse + pntSpecular;
		}
	}

	outFragColor = vec4(lighting, 1.0f);
}