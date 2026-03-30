#version 450

layout(binding = 0, std140) uniform CameraUBO
{
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 ambient;
    vec4 skyTint;
    vec4 renderSettings;
    vec4 lightingSettings;
    vec4 cameraForward;
} camera;

struct PushConstants
{
    mat4 model;
    vec4 color;
    vec4 options;
    vec4 outlineColor;
    vec4 outlineParams;
};

uniform PushConstants pushData;

layout(location = 0) out vec2 fragUv;
layout(location = 0) in vec3 inPosition;
layout(location = 1) out vec4 fragColor;
layout(location = 1) in vec3 inNormal;

void main()
{
    fragUv = vec2(inPosition.x + 0.5, 1.0 - (inPosition.y + 0.5));
    fragColor = pushData.color;
    vec4 worldPos = pushData.model * vec4(inPosition, 1.0);
    gl_Position = (camera.projection * camera.view) * worldPos;
}

