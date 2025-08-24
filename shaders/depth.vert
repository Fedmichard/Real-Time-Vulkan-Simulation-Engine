#version 450

// Fullscreen quad positions + UVs
layout(location = 0) in vec2 inPos;   // e.g. (-1,-1), (1,-1), (1,1), (-1,1)
layout(location = 1) in vec2 inUV;    // corresponding UVs (0,0), (1,0), (1,1), (0,1)

layout(location = 0) out vec2 fragUV;

void main() {
    fragUV = inUV;
    gl_Position = vec4(inPos, 0.0, 1.0);
}