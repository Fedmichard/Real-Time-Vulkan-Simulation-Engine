struct GPULight {
    vec4 pos;
    vec4 color;
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
} sceneData;

layout(set = 1, binding = 0) uniform OpenGLMaterialData {
    vec4 colorFactors;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
} materialData;