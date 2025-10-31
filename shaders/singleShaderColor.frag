#version 450

#extension GL_GOOGLE_include_directive : require

layout (location = 0) in vec3 inFragPos;

layout (location = 0) out vec4 outFragColor;

void main() {
    outFragColor = vec4(1.0, 1.0, 0.0, 1.0);
}