#version 450

layout(binding = 0, std140) uniform ShadowUBO
{
    mat4 lightViewProjection[3];
} shadow;

struct ShadowPush
{
    mat4 model;
    vec4 cascade;
};

uniform ShadowPush pushData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

void main()
{
    int cascadeIndex = int(pushData.cascade.x + 0.5);
    cascadeIndex = clamp(cascadeIndex, 0, 2);
    gl_Position = (shadow.lightViewProjection[cascadeIndex] * pushData.model) * vec4(inPosition, 1.0);
}

