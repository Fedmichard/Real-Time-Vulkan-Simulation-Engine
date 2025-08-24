#version 450

layout(set = 0, binding = 0) uniform sampler2D depthSampler;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
    float depth = texture(depthSampler, fragUV).r;
    outColor = vec4(depth, depth, depth, 1.0); // Explicitly set RGB to the same value
}