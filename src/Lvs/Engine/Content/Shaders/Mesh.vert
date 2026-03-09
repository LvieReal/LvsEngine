#version 450

// TEMPORARY: this Mesh pipeline is a bring-up shader.
// It will be replaced by Main.* once full scene/material plumbing is wired.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 0) uniform CameraUBO {
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

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 baseColor;
    vec4 material;
    vec4 surfaceData0;
    vec4 surfaceData1;
} pushData;

layout(location = 0) out vec3 fragNormal;

void main() {
    mat3 normalMatrix = mat3(pushData.model);
    fragNormal = normalize(normalMatrix * inNormal);
    gl_Position = camera.projection * camera.view * pushData.model * vec4(inPosition, 1.0);
}
