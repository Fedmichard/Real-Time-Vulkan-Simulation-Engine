#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

struct GPULight {
    vec4 pos;
    vec4 color;
    vec4 direction;
	vec3 padding_;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
};

layout(set = 0, binding = 0) uniform  SceneData {   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	vec4 cameraPos;
    GPULight emitter[16];
    float emitterCount;
} sceneData;

layout(set = 1, binding = 0) uniform GLTFMaterialData{   
	vec4 dummy;
} materialData;

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
}