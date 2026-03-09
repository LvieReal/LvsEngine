#version 450

layout(binding = 0, std140) uniform CameraUBO
{
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 lightDirection;
    vec4 lightColorIntensity;
    vec4 lightSpecular;
    vec4 ambient;
    vec4 skyTint;
    vec4 renderSettings;
    mat4 shadowMatrices[3];
    vec4 shadowCascadeSplits;
    vec4 shadowParams;
    vec4 shadowState;
    vec4 cameraForward;
} camera;

struct PushConstants
{
    mat4 model;
    vec4 baseColor;
    vec4 material;
    vec4 surfaceData0;
    vec4 surfaceData1;
};

uniform PushConstants pushData;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) in vec3 inNormal;
layout(location = 0) in vec3 inPosition;

void main()
{
    mat3 normalMatrix = mat3(pushData.model[0].xyz, pushData.model[1].xyz, pushData.model[2].xyz);
    fragNormal = normalize(normalMatrix * inNormal);
    gl_Position = ((camera.projection * camera.view) * pushData.model) * vec4(inPosition, 1.0);
}

