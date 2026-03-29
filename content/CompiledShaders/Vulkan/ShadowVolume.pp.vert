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

layout(push_constant) uniform ShadowVolumePush {
    uvec4 data;          // x: base instance
    vec4 lightDirExtrude; // xyz: light ray direction (world), w: extrude distance
    vec4 params;         // x: bias (world units)
} pushData;

layout(location = 0) out vec3 vWorldPos;

void main() {
    uint instanceIndex = pushData.data.x + gl_InstanceIndex;
    InstanceData inst = instanceData.instances[instanceIndex];
    vec4 world = inst.model * vec4(inPosition, 1.0);
    vWorldPos = world.xyz;
    gl_Position = camera.projection * camera.view * world;
}

