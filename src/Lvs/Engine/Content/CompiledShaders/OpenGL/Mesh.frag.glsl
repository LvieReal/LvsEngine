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

layout(location = 0) in vec3 fragNormal;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 n = normalize(fragNormal);
    vec3 l = normalize(-camera.lightDirection.xyz);
    float ndotl = max(dot(n, l), 0.0);
    vec3 litColor = pushData.baseColor.xyz * (camera.ambient.xyz + (camera.lightColorIntensity.xyz * ndotl));
    outColor = vec4(litColor, pushData.baseColor.w);
}

