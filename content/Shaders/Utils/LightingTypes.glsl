#pragma once

#ifndef LVS_UTIL_LIGHTING_TYPES_GLSL
#define LVS_UTIL_LIGHTING_TYPES_GLSL

struct Light {
    uint type;
    uint flags;
    uint dataIndex;
    uint shadowIndex;
    vec4 colorIntensity;
    vec4 specular; // x: strength, y: shininess, z: fresnelAmount, w: highlightType
};

struct DirectionalLight {
    vec4 direction;
    vec4 shadowCascadeSplits;
    vec4 shadowParams;
    vec4 shadowBiasParams;
    vec4 shadowState;
    mat4 shadowMatrices[3];
    mat4 shadowInvMatrices[3];
};

layout(set = 0, binding = 10, std430) readonly buffer LightsSSBO {
    uvec4 counts; // x: lightCount
    Light lights[64];
    DirectionalLight directionalLights[64];
} lightData;

#endif

