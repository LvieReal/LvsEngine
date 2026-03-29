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

layout(binding = 0, std140) uniform ShadowUBO
{
    mat4 lightViewProjection[3];
} shadow;

struct ShadowPush
{
    uvec4 data;
};

uniform ShadowPush pushData;

#ifdef GL_ARB_shader_draw_parameters
#define SPIRV_Cross_BaseInstance gl_BaseInstanceARB
#else
uniform int SPIRV_Cross_BaseInstance;
#endif
layout(location = 0) in vec3 inPosition;
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
    int cascadeIndex = int(pushData.data.y);
    cascadeIndex = clamp(cascadeIndex, 0, 2);
    gl_Position = (shadow.lightViewProjection[cascadeIndex] * inst.model) * vec4(inPosition, 1.0);
}

