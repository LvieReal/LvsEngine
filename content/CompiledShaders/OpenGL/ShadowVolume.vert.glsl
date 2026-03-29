#version 450
#ifdef GL_ARB_shader_draw_parameters
#extension GL_ARB_shader_draw_parameters : enable
#endif

struct InstanceData
{
    mat4 model;
    vec4 baseColor;
    vec4 material;
    vec4 surfaceData0;
    vec4 surfaceData1;
};

layout(binding = 9, std430) readonly buffer InstanceSSBO
{
    InstanceData instances[];
} instanceData;

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

struct ShadowVolumePush
{
    uvec4 data;
    vec4 lightDirExtrude;
    vec4 params;
};

uniform ShadowVolumePush pushData;

#ifdef GL_ARB_shader_draw_parameters
#define SPIRV_Cross_BaseInstance gl_BaseInstanceARB
#else
uniform int SPIRV_Cross_BaseInstance;
#endif
layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 vWorldPos;
layout(location = 1) in vec3 inNormal;

void main()
{
    uint instanceIndex = pushData.data.x + uint((gl_InstanceID + SPIRV_Cross_BaseInstance));
    InstanceData inst;
    inst.model = instanceData.instances[instanceIndex].model;
    inst.baseColor = instanceData.instances[instanceIndex].baseColor;
    inst.material = instanceData.instances[instanceIndex].material;
    inst.surfaceData0 = instanceData.instances[instanceIndex].surfaceData0;
    inst.surfaceData1 = instanceData.instances[instanceIndex].surfaceData1;
    vec4 world = inst.model * vec4(inPosition, 1.0);
    vWorldPos = world.xyz;
    gl_Position = (camera.projection * camera.view) * world;
}

