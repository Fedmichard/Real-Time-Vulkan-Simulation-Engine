#version 450

#extension GL_GOOGLE_include_directive : require

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inFragPos;

layout (location = 0) out vec4 outFragColor;

void main()  {
    // Remap normals from [-1, 1] to [0, 1] so negative values are visible
    // multiplying by 0.5 cuts all values in half (maximum is now 0.5 and minimum is -0.5)
    // then you add +0.5 so minimum is 0.0 and the max is 1.0
    outFragColor = vec4(inNormal * 0.5 + 0.5, 1.0f);
}