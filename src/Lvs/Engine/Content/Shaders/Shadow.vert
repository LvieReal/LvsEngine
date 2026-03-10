#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 0) uniform ShadowUBO {
    mat4 lightViewProjection[3];
} shadow;

layout(push_constant) uniform ShadowPush {
    mat4 model;
    vec4 cascade; // cascade.x stores cascade index for this pass
} pushData;

void main() {
    int cascadeIndex = int(pushData.cascade.x + 0.5);
    cascadeIndex = clamp(cascadeIndex, 0, 2);
    gl_Position = shadow.lightViewProjection[cascadeIndex] * pushData.model * vec4(inPosition, 1.0);
}
