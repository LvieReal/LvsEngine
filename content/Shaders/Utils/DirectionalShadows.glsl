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

ivec2 GetDirectionalShadowMapSize(int shadowMapBase, int cascadeIndex) {
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    return max(textureSize(directionalShadowMaps[idx], 0), ivec2(1));
}

float SampleDirectionalShadowMap(int shadowMapBase, int cascadeIndex, vec3 shadowCoord) {
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    return texture(directionalShadowMaps[idx], shadowCoord);
}

float SampleDirectionalShadowHard(int shadowMapBase, int cascadeIndex, vec2 uv, float compareDepth) {
    vec2 cuv = clamp(uv, vec2(0.0), vec2(1.0));
    return SampleDirectionalShadowMap(shadowMapBase, cascadeIndex, vec3(cuv, compareDepth)) > 0.5 ? 1.0 : 0.0;
}

float SampleDirectionalShadowHardTexel(int shadowMapBase, int cascadeIndex, ivec2 texel, ivec2 shadowSize, float compareDepth) {
    ivec2 clamped = clamp(texel, ivec2(0), shadowSize - ivec2(1));
    vec2 uv = (vec2(clamped) + 0.5) / vec2(shadowSize);
    return SampleDirectionalShadowHard(shadowMapBase, cascadeIndex, uv, compareDepth);
}

float ShadowTestRBSM(int shadowMapBase, int cascadeIndex, vec3 p) {
    return SampleDirectionalShadowHard(shadowMapBase, cascadeIndex, p.xy, p.z);
}

vec4 ComputeDiscontinuity4_RBSM(int shadowMapBase, int cascadeIndex, vec3 p, vec2 o) {
    float center = ShadowTestRBSM(shadowMapBase, cascadeIndex, p);
    float left = ShadowTestRBSM(shadowMapBase, cascadeIndex, vec3(p.x - o.x, p.y, p.z));
    float right = ShadowTestRBSM(shadowMapBase, cascadeIndex, vec3(p.x + o.x, p.y, p.z));
    float bottom = ShadowTestRBSM(shadowMapBase, cascadeIndex, vec3(p.x, p.y - o.y, p.z));
    float top = ShadowTestRBSM(shadowMapBase, cascadeIndex, vec3(p.x, p.y + o.y, p.z));
    return abs(vec4(left, right, bottom, top) - center);
}

float ComputeRelativeDistance1_RBSM(int shadowMapBase, int cascadeIndex, vec3 p, vec2 dir, float c, vec2 o) {
    const int maxSize = 16;
    vec3 np = p;
    float foundSilhouetteEnd = 0.0;
    float distance = 0.0;
    vec2 stepUv = dir * o;

    for (int it = 0; it < maxSize; ++it) {
        np.xy += stepUv;
        if (np.x < 0.0 || np.x > 1.0 || np.y < 0.0 || np.y > 1.0) {
            break;
        }

        float center = ShadowTestRBSM(shadowMapBase, cascadeIndex, np);
        bool isCenterUmbra = center < 0.5;
        if (isCenterUmbra) {
            foundSilhouetteEnd = 1.0;
            break;
        }

        vec4 d = ComputeDiscontinuity4_RBSM(shadowMapBase, cascadeIndex, np, o);
        if ((d.x + d.y + d.z + d.w) == 0.0) {
            break;
        }

        distance += 1.0;
    }

    distance = distance + (1.0 - c);
    return mix(-distance, distance, foundSilhouetteEnd);
}

vec4 ComputeRelativeDistance4_RBSM(int shadowMapBase, int cascadeIndex, vec3 p, vec2 c, vec2 o) {
    float dl = ComputeRelativeDistance1_RBSM(shadowMapBase, cascadeIndex, p, vec2(-1.0, 0.0), (1.0 - c.x), o);
    float dr = ComputeRelativeDistance1_RBSM(shadowMapBase, cascadeIndex, p, vec2(1.0, 0.0), c.x, o);
    vec2 bottomDir = vec2(0.0, -1.0);
    vec2 topDir = vec2(0.0, 1.0);
    float db = ComputeRelativeDistance1_RBSM(shadowMapBase, cascadeIndex, p, bottomDir, (1.0 - c.y), o);
    float dt = ComputeRelativeDistance1_RBSM(shadowMapBase, cascadeIndex, p, topDir, c.y, o);
    return vec4(dl, dr, db, dt);
}

float NormalizeRelativeDistance1_RBSM(vec2 dist) {
    const int maxSize = 16;
    float T = 1.0;
    if (dist.x < 0.0 && dist.y < 0.0) T = 0.0;
    if (dist.x > 0.0 && dist.y > 0.0) T = -2.0;
    float len = min(abs(dist.x) + abs(dist.y), float(maxSize));
    return (len > 1e-6) ? (abs(max(T * dist.x, T * dist.y)) / len) : 0.0;
}

vec2 NormalizeRelativeDistance2_RBSM(vec4 dist) {
    return vec2(
        NormalizeRelativeDistance1_RBSM(vec2(dist.x, dist.y)),
        NormalizeRelativeDistance1_RBSM(vec2(dist.z, dist.w))
    );
}

float RevectorizeShadow_RBSM(vec2 r) {
    // Conservative RBSM: return 0 for new shadow, 1 otherwise.
    if ((r.x * r.y > 0.0) && ((1.0 - r.x) > r.y)) return 0.0;
    return 1.0;
}

float SampleDirectionalRBSM(int shadowMapBase, int cascadeIndex, vec2 shadowUv, float compareDepth) {
    ivec2 shadowSize = GetDirectionalShadowMapSize(shadowMapBase, cascadeIndex);
    vec2 o = 1.0 / vec2(shadowSize);
    vec3 p = vec3(shadowUv, compareDepth);

    float shadowTestResult = ShadowTestRBSM(shadowMapBase, cascadeIndex, p);
    if (shadowTestResult < 0.5) {
        return 0.0;
    }

    vec2 c = fract(shadowUv * vec2(shadowSize));
    vec4 disc = ComputeDiscontinuity4_RBSM(shadowMapBase, cascadeIndex, p, o);
    if ((disc.x + disc.y + disc.z + disc.w) > 0.0) {
        vec4 dist = ComputeRelativeDistance4_RBSM(shadowMapBase, cascadeIndex, p, c, o);
        vec2 r = NormalizeRelativeDistance2_RBSM(dist);
        return RevectorizeShadow_RBSM(r);
    }

    return shadowTestResult;
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

    float depthBiasTexels = max(dl.shadowParams.x, 0.0);
    return depthBiasTexels * texelScale;
}

vec3 ApplyDirectionalNormalOffset(
    DirectionalLight dl,
    int cascadeIndex,
    vec3 worldPos,
    vec3 normal,
    vec2 shadowUv,
    float receiverDepth,
    vec2 texelSize
) {
    float normalOffsetTexels = max(dl.shadowBiasParams.x, 0.0);
    if (normalOffsetTexels <= 1e-6) {
        return worldPos;
    }

    vec3 ndc0 = vec3((shadowUv * 2.0) - 1.0, receiverDepth);
    vec3 ndcX = vec3(((shadowUv + vec2(texelSize.x, 0.0)) * 2.0) - 1.0, receiverDepth);
    vec3 ndcY = vec3(((shadowUv + vec2(0.0, texelSize.y)) * 2.0) - 1.0, receiverDepth);

    vec4 w0 = dl.shadowInvMatrices[cascadeIndex] * vec4(ndc0, 1.0);
    vec4 wX = dl.shadowInvMatrices[cascadeIndex] * vec4(ndcX, 1.0);
    vec4 wY = dl.shadowInvMatrices[cascadeIndex] * vec4(ndcY, 1.0);

    float invW0 = 1.0 / max(abs(w0.w), 1e-6);
    float invWX = 1.0 / max(abs(wX.w), 1e-6);
    float invWY = 1.0 / max(abs(wY.w), 1e-6);

    vec3 p0 = w0.xyz * invW0;
    vec3 pX = wX.xyz * invWX;
    vec3 pY = wY.xyz * invWY;

    float worldPerTexel = max(length(pX - p0), length(pY - p0));
    float offsetWorld = normalOffsetTexels * worldPerTexel;
    return worldPos + (normalize(normal) * offsetWorld);
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

    vec2 texelSize = GetDirectionalShadowTexelSize(shadowMapBase, cascadeIndex);
    vec3 biasedWorldPos = ApplyDirectionalNormalOffset(
        dl,
        cascadeIndex,
        worldPos,
        normal,
        shadowUv,
        receiverDepth,
        texelSize
    );
    if (distance(biasedWorldPos, worldPos) > 1e-7) {
        lightClip = dl.shadowMatrices[cascadeIndex] * vec4(biasedWorldPos, 1.0);
        if (abs(lightClip.w) <= 1e-6) {
            return 1.0;
        }
        lightNdc = lightClip.xyz / lightClip.w;
        shadowUv = (lightNdc.xy * 0.5) + 0.5;
        receiverDepth = lightNdc.z;
        if (receiverDepth <= 0.0 || receiverDepth >= 1.0) {
            return 1.0;
        }
        if (shadowUv.x < 0.0 || shadowUv.x > 1.0 || shadowUv.y < 0.0 || shadowUv.y > 1.0) {
            return 1.0;
        }
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
    float shadow = 1.0;
    if (dl.shadowState.w > 0.5) {
        // RBSM silhouette recovery variant (binary visibility).
        shadow = SampleDirectionalRBSM(shadowMapBase, cascadeIndex, shadowUv, compareDepth);
    } else {
        shadow = SampleDirectionalPCF(
            dl,
            shadowMapBase,
            cascadeIndex,
            vec3(shadowUv, compareDepth),
            softness,
            normal,
            lightDir,
            fragCoord
        );
    }

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
