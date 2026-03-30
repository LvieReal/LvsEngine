#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 ambient;
    vec4 skyTint;
    vec4 renderSettings;
    vec4 lightingSettings;
    vec4 cameraForward;
} camera;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    vec4 options; // x: negateMask, y: depthOnly, z: outlineEnabled
    vec4 outlineColor; // rgb color, a alpha
    vec4 outlineParams; // x thicknessPixels, y alphaThreshold
} pushData;

layout(location = 0) out vec2 fragUv;
layout(location = 1) out vec4 fragColor;

void main() {
    fragUv = vec2(inPosition.x + 0.5, 1.0 - (inPosition.y + 0.5));
    fragColor = pushData.color;

    vec4 worldPos = pushData.model * vec4(inPosition, 1.0);
    gl_Position = camera.projection * camera.view * worldPos;
}
