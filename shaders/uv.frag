#version 450

#extension GL_GOOGLE_include_directive : require

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inFragPos;

layout (location = 0) out vec4 outFragColor;

void main()  {
    // Use fract() to visualize tiling. 
    // If UVs are > 1.0, this wraps them back to 0..1 range visually.
    outFragColor = vec4(fract(inUV), 0.0f, 1.0f);
}