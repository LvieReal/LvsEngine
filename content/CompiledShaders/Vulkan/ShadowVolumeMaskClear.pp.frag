#version 450

layout(location = 0) in vec2 fragUv;

layout(location = 3) out vec4 outShadowMask;

void main() {
    outShadowMask = vec4(0.0, 0.0, 0.0, 1.0);
}


