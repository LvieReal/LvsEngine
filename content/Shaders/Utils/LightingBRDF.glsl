#pragma once

#ifndef LVS_UTIL_LIGHTING_BRDF_GLSL
#define LVS_UTIL_LIGHTING_BRDF_GLSL

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 1e-5);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / max(denom, 1e-5);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

uint GetSpecularHighlightType(vec4 lightSpecular) {
    uint t = uint(lightSpecular.w + 0.5);
    if (t < 1u || t > 3u) {
        t = 3u;
    }
    return t;
}

vec3 SpecularPhong(vec3 N, vec3 V, vec3 L, vec3 F, float shininess) {
    vec3 R = reflect(-L, N);
    float s = pow(max(dot(R, V), 0.0), shininess);
    return F * s;
}

vec3 SpecularBlinnPhong(vec3 N, vec3 H, vec3 F, float shininess) {
    float s = pow(max(dot(N, H), 0.0), shininess);
    return F * s;
}

vec3 SpecularCookTorrance(vec3 N, vec3 V, vec3 L, vec3 H, vec3 F, float effectiveRoughness) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    float D = DistributionGGX(N, H, effectiveRoughness);
    float G = GeometrySmith(N, V, L, effectiveRoughness);
    vec3 numerator = D * G * F;
    float denominator = max(4.0 * NdotV * NdotL, 1e-5);
    return numerator / denominator;
}

#endif

