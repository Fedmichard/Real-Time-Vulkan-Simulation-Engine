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
	vec4 colorFactors;
} materialData;