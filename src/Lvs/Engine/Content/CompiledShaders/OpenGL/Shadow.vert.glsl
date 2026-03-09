#version 450

struct ShadowPushConstants
{
    mat4 lightViewProjection;
    mat4 model;
};

uniform ShadowPushConstants pushData;

layout(location = 0) in vec3 inPosition;

void main()
{
    gl_Position = (pushData.lightViewProjection * pushData.model) * vec4(inPosition, 1.0);
}

