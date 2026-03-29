#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 0) uniform ShadowUBO {
    mat4 lightViewProjection[3];
} shadow;

struct InstanceData {
    mat4 model;
    vec4 baseColor;
    vec4 material;
    vec4 surfaceData0;
    vec4 surfaceData1;
};

layout(set = 0, binding = 9, std430) readonly buffer InstanceSSBO {
    InstanceData instances[];
} instanceData;

layout(push_constant) uniform ShadowPush {
    uvec4 data; // x: base instance, y: cascade index
} pushData;

void main() {
    uint instanceIndex = pushData.data.x + gl_InstanceIndex;
    InstanceData inst = instanceData.instances[instanceIndex];

    int cascadeIndex = int(pushData.data.y);
    cascadeIndex = clamp(cascadeIndex, 0, 2);
    gl_Position = shadow.lightViewProjection[cascadeIndex] * inst.model * vec4(inPosition, 1.0);
}
