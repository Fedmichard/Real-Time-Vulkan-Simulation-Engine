#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outFragPos;

struct Vertex {
	vec3 position;
	float uvX;
	vec3 normal;
	float uvY;
	vec4 color;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//push constants block
layout( push_constant ) uniform constants {
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
} PushConstants;

void main()  {
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);

	vec4 worldPos = PushConstants.render_matrix * position;

	gl_Position =  sceneData.viewproj * PushConstants.render_matrix * position;

	mat3 normalMatrix = transpose(inverse(mat3(PushConstants.render_matrix)));
	outNormal = normalize(normalMatrix * v.normal);
	outColor = v.color.xyz * materialData.colorFactors.xyz;	
	outUV.x = v.uvX;
	outUV.y = v.uvY;
	outFragPos = worldPos.xyz;
}