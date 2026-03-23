#pragma once

#ifndef LVS_UTIL_DIRECTIONAL_SHADOWS_GLSL
#define LVS_UTIL_DIRECTIONAL_SHADOWS_GLSL

const int SHADOW_SAMPLES_COUNT = 64;
const int SHADOW_SAMPLES_HALF = SHADOW_SAMPLES_COUNT / 2;
const float INV_SHADOW_SAMPLES = 1.0 / float(SHADOW_SAMPLES_COUNT);

vec2 GetDirectionalShadowTexelSize(int shadowMapBase, int cascadeIndex) {
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    ivec2 size = max(textureSize(directionalShadowMaps[idx], 0), ivec2(1));
    return 1.0 / vec2(size);
}

float SampleDirectionalShadowMap(int shadowMapBase, int cascadeIndex, vec3 shadowCoord) {
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    return texture(directionalShadowMaps[idx], shadowCoord);
}

float SampleDirectionalPCF(
    DirectionalLight dl,
    int shadowMapBase,
    int cascadeIndex,
    vec3 baseShadowCoord,
    float radiusTexels,
    vec3 normal,
    vec3 lightDir,
    vec2 fragCoord
) {
    int desiredTaps = clamp(int(dl.shadowParams.z + 0.5), 1, SHADOW_SAMPLES_COUNT);
    if (desiredTaps <= 1) {
        return SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, baseShadowCoord);
    }

    vec2 texelSize = GetDirectionalShadowTexelSize(shadowMapBase, cascadeIndex);
    float radius = max(radiusTexels, 0.0);

    float ndotl = max(dot(normalize(normal), normalize(lightDir)), 0.0);
    vec3 jcoord = vec3(fragCoord * dl.shadowState.yz, 0.0);
    float shadowSum = 0.0;

    // Cheap pretest: up to 8 taps via jitter fetches.
    int pretestTaps = min(desiredTaps, 8);
    for (int i = 0; i < SHADOW_SAMPLES_HALF && (i * 2) < pretestTaps; ++i) {
        vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - 1.0;
        jcoord.z += 1.0 / float(SHADOW_SAMPLES_HALF);

        vec3 smCoord = baseShadowCoord;
        vec2 uvOffset = offset.xy * texelSize * radius;
        smCoord.xy = uvOffset + baseShadowCoord.xy;
        shadowSum += SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, smCoord);

        if ((i * 2 + 1) < pretestTaps) {
            uvOffset = offset.zw * texelSize * radius;
            smCoord.xy = uvOffset + baseShadowCoord.xy;
            shadowSum += SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, smCoord);
        }
    }

    float shadowPretest = shadowSum / float(max(pretestTaps, 1));
    if (desiredTaps <= pretestTaps) {
        return shadowPretest;
    }

    // Only refine in the penumbra.
    if (ndotl > 0.0 && shadowPretest > 0.0 && shadowPretest < 1.0) {
        int tapsDone = pretestTaps;
        for (int i = 0; i < SHADOW_SAMPLES_HALF && tapsDone < desiredTaps; ++i) {
            vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - 1.0;
            jcoord.z += 1.0 / float(SHADOW_SAMPLES_HALF);

            vec3 smCoord = baseShadowCoord;
            vec2 uvOffset = offset.xy * texelSize * radius;
            smCoord.xy = uvOffset + baseShadowCoord.xy;
            shadowSum += SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, smCoord);
            tapsDone += 1;

            if (tapsDone < desiredTaps) {
                uvOffset = offset.zw * texelSize * radius;
                smCoord.xy = uvOffset + baseShadowCoord.xy;
                shadowSum += SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, smCoord);
                tapsDone += 1;
            }
        }
        return shadowSum / float(desiredTaps);
    }

    return shadowPretest;
}

float ComputeDirectionalBias(DirectionalLight dl, int shadowMapBase, int cascadeIndex, vec3 normal, vec3 lightDir) {
    vec2 texelSize = GetDirectionalShadowTexelSize(shadowMapBase, cascadeIndex);
    float texelScale = max(texelSize.x, texelSize.y);

    float constantBiasTexels = max(dl.shadowParams.x, 0.0);
    float slopeBiasFactor = max(dl.shadowBiasParams.x, 0.0);
    float maxBiasTexels = max(dl.shadowBiasParams.y, 0.0);

    float ndotl = max(dot(normalize(normal), normalize(lightDir)), 0.0);
    float tanTheta = sqrt(max(1.0 - (ndotl * ndotl), 0.0)) / max(ndotl, 1e-3);
    float biasTexels = constantBiasTexels + (slopeBiasFactor * tanTheta);
    if (maxBiasTexels > 0.0) {
        biasTexels = min(biasTexels, maxBiasTexels);
    }
    return biasTexels * texelScale;
}

float ComputeDirectionalShadowFactor(
    DirectionalLight dl,
    int shadowMapBase,
    vec3 normal,
    vec3 lightDir,
    vec3 worldPos,
    vec3 cameraPos,
    vec3 cameraForward,
    vec2 fragCoord
) {
    if (dl.shadowState.x < 0.5) {
        return 1.0;
    }

    int cascadeCount = clamp(int(dl.shadowCascadeSplits.w + 0.5), 1, 3);
    vec3 fwd = normalize(cameraForward);
    float viewDepth = max(dot(worldPos - cameraPos, fwd), 0.0);
    int cascadeIndex = 0;
    if (cascadeCount >= 3 && viewDepth > dl.shadowCascadeSplits.y) {
        cascadeIndex = 2;
    } else if (cascadeCount >= 2 && viewDepth > dl.shadowCascadeSplits.x) {
        cascadeIndex = 1;
    }

    vec4 lightClip = dl.shadowMatrices[cascadeIndex] * vec4(worldPos, 1.0);
    if (abs(lightClip.w) <= 1e-6) {
        return 1.0;
    }

    vec3 lightNdc = lightClip.xyz / lightClip.w;
    vec2 shadowUv = (lightNdc.xy * 0.5) + 0.5;
    float receiverDepth = lightNdc.z;
    if (receiverDepth <= 0.0 || receiverDepth >= 1.0) {
        return 1.0;
    }
    if (shadowUv.x < 0.0 || shadowUv.x > 1.0 || shadowUv.y < 0.0 || shadowUv.y > 1.0) {
        return 1.0;
    }

    float softness = dl.shadowParams.y;
    vec2 baseSize = vec2(max(textureSize(directionalShadowMaps[clamp(shadowMapBase + 0, 0, 5)], 0), ivec2(1)));
    vec2 cascadeSize = baseSize;
    cascadeSize = vec2(max(textureSize(directionalShadowMaps[clamp(shadowMapBase + cascadeIndex, 0, 5)], 0), ivec2(1)));
    float baseRes = max(baseSize.x, baseSize.y);
    float cascadeRes = max(cascadeSize.x, cascadeSize.y);
    float resScale = cascadeRes / max(baseRes, 1.0);
    softness *= resScale;
    float bias = ComputeDirectionalBias(dl, shadowMapBase, cascadeIndex, normal, lightDir);
    float compareDepth = clamp(receiverDepth - bias, 0.0, 1.0);
    float shadow = SampleDirectionalPCF(
        dl,
        shadowMapBase,
        cascadeIndex,
        vec3(shadowUv, compareDepth),
        softness,
        normal,
        lightDir,
        fragCoord
    );

    if (cascadeIndex == cascadeCount - 1) {
        float fadeWidth = clamp(dl.shadowParams.w, 0.0, 1.0);
        if (fadeWidth > 1e-4) {
            float fadeStart = dl.shadowCascadeSplits.z * (1.0 - fadeWidth);
            float fade = clamp((viewDepth - fadeStart) / max(dl.shadowCascadeSplits.z - fadeStart, 1e-4), 0.0, 1.0);
            shadow = mix(shadow, 1.0, fade);
        }
    }
    return shadow;
}

#endif

