#version 450

layout(location = 3) out vec4 outShadowMask;
layout(location = 0) in vec2 fragUv;

void main()
{
    outShadowMask = vec4(1.0, 0.0, 0.0, 1.0);
}

