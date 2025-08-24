#version 450

layout(set = 0, binding = 0) uniform sampler2D depthSampler;

layout(location = 0) in vec2 fragUV; // passed from fullscreen quad
layout(location = 0) out vec4 outColor;

void main() {
    float depth = texture(depthSampler, fragUV).r; 
    outColor = vec4(vec3(depth), 1.0); // grayscale
}