#version 450

layout(binding = 0, std140) uniform CameraUBO
{
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 lightDirection;
    vec4 lightColorIntensity;
    vec4 lightSpecular;
    vec4 ambient;
    vec4 skyTint;
    vec4 renderSettings;
    mat4 shadowMatrices[3];
    vec4 shadowCascadeSplits;
    vec4 shadowParams;
    vec4 shadowState;
    vec4 cameraForward;
} camera;

struct PushConstants
{
    mat4 model;
    vec4 baseColor;
    vec4 material;
    vec4 surfaceData0;
    vec4 surfaceData1;
};

uniform PushConstants pushData;

layout(binding = 4) uniform sampler2D surfaceAtlas;
layout(binding = 5) uniform sampler2D surfaceNormalAtlas;
layout(binding = 2) uniform sampler2DShadow directionalShadowMaps[3];
layout(binding = 3) uniform sampler3D directionalShadowJitter;
layout(binding = 1) uniform samplerCube skyboxTex;

layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec4 fragBaseColor;
layout(location = 6) in vec3 fragLocalNormal;
layout(location = 5) in vec3 fragLocalPos;
layout(location = 3) in vec4 fragMaterial;
layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outGlowColor;
layout(location = 4) in vec3 fragVertexLighting;
layout(location = 0) in vec3 fragNormal;

int GetTopSurfaceType()
{
    return int(pushData.surfaceData0.x + 0.5);
}

int GetBottomSurfaceType()
{
    return int(pushData.surfaceData0.y + 0.5);
}

int GetRightSurfaceType()
{
    return int(pushData.surfaceData1.y + 0.5);
}

int GetLeftSurfaceType()
{
    return int(pushData.surfaceData1.x + 0.5);
}

int GetFrontSurfaceType()
{
    return int(pushData.surfaceData0.z + 0.5);
}

int GetBackSurfaceType()
{
    return int(pushData.surfaceData0.w + 0.5);
}

int GetSurfaceType(vec3 normal)
{
    vec3 n = normalize(normal);
    vec3 a = abs(n);
    bool _323 = a.y >= a.x;
    bool _331;
    if (_323)
    {
        _331 = a.y >= a.z;
    }
    else
    {
        _331 = _323;
    }
    if (_331)
    {
        int _337;
        if (n.y > 0.0)
        {
            _337 = GetTopSurfaceType();
        }
        else
        {
            _337 = GetBottomSurfaceType();
        }
        return _337;
    }
    bool _349 = a.x >= a.y;
    bool _357;
    if (_349)
    {
        _357 = a.x >= a.z;
    }
    else
    {
        _357 = _349;
    }
    if (_357)
    {
        int _363;
        if (n.x > 0.0)
        {
            _363 = GetRightSurfaceType();
        }
        else
        {
            _363 = GetLeftSurfaceType();
        }
        return _363;
    }
    int _374;
    if (n.z > 0.0)
    {
        _374 = GetFrontSurfaceType();
    }
    else
    {
        _374 = GetBackSurfaceType();
    }
    return _374;
}

vec3 GetMeshSizeFromModel()
{
    vec3 xAxis = vec3(pushData.model[0].x, pushData.model[0].y, pushData.model[0].z);
    vec3 yAxis = vec3(pushData.model[1].x, pushData.model[1].y, pushData.model[1].z);
    vec3 zAxis = vec3(pushData.model[2].x, pushData.model[2].y, pushData.model[2].z);
    return vec3(length(xAxis), length(yAxis), length(zAxis));
}

vec2 GetFaceUV(vec3 localPos, vec3 localNormal)
{
    vec3 scaledPos = localPos * GetMeshSizeFromModel();
    vec3 n = normalize(localNormal);
    vec3 a = abs(n);
    bool _397 = a.x >= a.y;
    bool _405;
    if (_397)
    {
        _405 = a.x >= a.z;
    }
    else
    {
        _405 = _397;
    }
    vec2 uv;
    if (_405)
    {
        uv = scaledPos.zy;
    }
    else
    {
        bool _416 = a.y >= a.x;
        bool _424;
        if (_416)
        {
            _424 = a.y >= a.z;
        }
        else
        {
            _424 = _416;
        }
        if (_424)
        {
            uv = scaledPos.xz;
        }
        else
        {
            uv = scaledPos.xy;
        }
    }
    return fract(uv * vec2(0.5));
}

bool IsSurfaceEnabled()
{
    return pushData.surfaceData1.z > 0.5;
}

vec2 GetSurfaceAtlasGrid()
{
    vec2 atlasSize = vec2(textureSize(surfaceAtlas, 0));
    float cols = max(1.0, round(atlasSize.x / max(atlasSize.y, 1.0)));
    return vec2(cols, 1.0);
}

vec2 GetSurfaceAtlasUV(int surfaceType, vec2 uv)
{
    vec2 grid = GetSurfaceAtlasGrid();
    float cols = max(1.0, grid.x);
    float rows = max(1.0, grid.y);
    float tileWidth = 1.0 / cols;
    float tileHeight = 1.0 / rows;
    float tileIndex = clamp(float(surfaceType - 1), 0.0, (cols * rows) - 1.0);
    float tileX = mod(tileIndex, cols);
    float tileY = floor(tileIndex / cols);
    vec2 base = vec2(tileX * tileWidth, tileY * tileHeight);
    vec2 tileUV = uv * vec2(tileWidth, tileHeight);
    return base + tileUV;
}

vec3 SampleSurfaceColor(int surfaceType, vec2 uv)
{
    bool _515 = surfaceType == 0;
    bool _521;
    if (!_515)
    {
        _521 = !IsSurfaceEnabled();
    }
    else
    {
        _521 = _515;
    }
    if (_521)
    {
        return vec3(1.0);
    }
    int param = surfaceType;
    vec2 param_1 = uv;
    return texture(surfaceAtlas, GetSurfaceAtlasUV(param, param_1)).xyz;
}

bool IsSurfaceNormalEnabled()
{
    return pushData.surfaceData1.w > 0.5;
}

vec3 GetSurfaceMappedNormal(vec3 localNormal, int surfaceType, vec2 uv)
{
    bool _537 = !IsSurfaceEnabled();
    bool _543;
    if (!_537)
    {
        _543 = !IsSurfaceNormalEnabled();
    }
    else
    {
        _543 = _537;
    }
    if (_543 || (surfaceType == 0))
    {
        return normalize(mat3(pushData.model[0].xyz, pushData.model[1].xyz, pushData.model[2].xyz) * localNormal);
    }
    vec3 n = normalize(localNormal);
    vec3 a = abs(n);
    bool _574 = a.x >= a.y;
    bool _582;
    if (_574)
    {
        _582 = a.x >= a.z;
    }
    else
    {
        _582 = _574;
    }
    vec3 tangentLocal;
    vec3 bitangentLocal;
    if (_582)
    {
        tangentLocal = vec3(0.0, 0.0, 1.0);
        bitangentLocal = vec3(0.0, 1.0, 0.0);
    }
    else
    {
        bool _594 = a.y >= a.x;
        bool _602;
        if (_594)
        {
            _602 = a.y >= a.z;
        }
        else
        {
            _602 = _594;
        }
        if (_602)
        {
            tangentLocal = vec3(1.0, 0.0, 0.0);
            bitangentLocal = vec3(0.0, 0.0, 1.0);
        }
        else
        {
            tangentLocal = vec3(1.0, 0.0, 0.0);
            bitangentLocal = vec3(0.0, 1.0, 0.0);
        }
    }
    int param = surfaceType;
    vec2 param_1 = uv;
    vec3 sampled = texture(surfaceNormalAtlas, GetSurfaceAtlasUV(param, param_1)).xyz;
    vec3 tangentSpaceNormal = normalize((sampled * 2.0) - vec3(1.0));
    vec3 mappedLocal = normalize(((tangentLocal * tangentSpaceNormal.x) + (bitangentLocal * tangentSpaceNormal.y)) + (n * tangentSpaceNormal.z));
    return normalize(mat3(pushData.model[0].xyz, pushData.model[1].xyz, pushData.model[2].xyz) * mappedLocal);
}

vec2 GetDirectionalShadowTexelSize(int cascadeIndex)
{
    return vec2(1.0) / vec2(textureSize(directionalShadowMaps[cascadeIndex], 0));
}

float ComputeDirectionalBias(int cascadeIndex, float pcfRadiusTexels, vec3 normal, vec3 lightDir)
{
    int param = cascadeIndex;
    vec2 texelSize = GetDirectionalShadowTexelSize(param);
    float texelScale = max(texelSize.x, texelSize.y);
    float cascadeResolution = 1.0 / max(texelScale, 9.9999999747524270787835121154785e-07);
    float ndotl = clamp(dot(normal, lightDir), 0.0, 1.0);
    float slope = sqrt(max(1.0 - (ndotl * ndotl), 0.0)) / max(ndotl, 0.0500000007450580596923828125);
    float biasTexels = camera.shadowParams.x + (0.25 * slope);
    biasTexels *= (1.0 + (pcfRadiusTexels * 0.25));
    float lowResRatio = clamp(2048.0 / max(cascadeResolution, 1.0), 1.0, 8.0);
    float lowResCompensation = sqrt(lowResRatio);
    return (biasTexels * texelScale) / lowResCompensation;
}

float SampleDirectionalShadowMap(int cascadeIndex, vec3 shadowCoord)
{
    return texture(directionalShadowMaps[cascadeIndex], vec3(shadowCoord.xy, shadowCoord.z));
}

float SampleDirectionalAdaptivePCF64(int cascadeIndex, vec3 baseShadowCoord, float radiusTexels, float ndotl)
{
    float radius = max(0.0, radiusTexels);
    if (radius <= 9.9999999747524270787835121154785e-07)
    {
        int param = cascadeIndex;
        vec3 param_1 = baseShadowCoord;
        return SampleDirectionalShadowMap(param, param_1);
    }
    int param_2 = cascadeIndex;
    vec2 texelSize = GetDirectionalShadowTexelSize(param_2);
    float texelScale = max(texelSize.x, texelSize.y);
    float fsize = radius * texelScale;
    vec3 jcoord = vec3(gl_FragCoord.xy * camera.shadowState.yz, 0.0);
    float shadow = 0.0;
    vec3 smCoord = baseShadowCoord;
    for (int i = 0; i < 4; i++)
    {
        vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - vec4(1.0);
        jcoord.z += 0.03125;
        vec2 _758 = (offset.xy * fsize) + baseShadowCoord.xy;
        smCoord.x = _758.x;
        smCoord.y = _758.y;
        int param_3 = cascadeIndex;
        vec3 param_4 = smCoord;
        shadow += (SampleDirectionalShadowMap(param_3, param_4) * 0.125);
        vec2 _778 = (offset.zw * fsize) + baseShadowCoord.xy;
        smCoord.x = _778.x;
        smCoord.y = _778.y;
        int param_5 = cascadeIndex;
        vec3 param_6 = smCoord;
        shadow += (SampleDirectionalShadowMap(param_5, param_6) * 0.125);
    }
    if (((ndotl > 0.0) && (shadow > 0.0)) && (shadow < 1.0))
    {
        shadow *= 0.125;
        for (int i_1 = 0; i_1 < 28; i_1++)
        {
            vec4 offset_1 = (texture(directionalShadowJitter, jcoord) * 2.0) - vec4(1.0);
            jcoord.z += 0.03125;
            vec2 _831 = (offset_1.xy * fsize) + baseShadowCoord.xy;
            smCoord.x = _831.x;
            smCoord.y = _831.y;
            int param_7 = cascadeIndex;
            vec3 param_8 = smCoord;
            shadow += (SampleDirectionalShadowMap(param_7, param_8) * 0.015625);
            vec2 _851 = (offset_1.zw * fsize) + baseShadowCoord.xy;
            smCoord.x = _851.x;
            smCoord.y = _851.y;
            int param_9 = cascadeIndex;
            vec3 param_10 = smCoord;
            shadow += (SampleDirectionalShadowMap(param_9, param_10) * 0.015625);
        }
    }
    return shadow;
}

float ComputeShadowFactor(vec3 normal, vec3 lightDir)
{
    if (camera.shadowState.x < 0.5)
    {
        return 1.0;
    }
    int cascadeCount = clamp(int(camera.shadowCascadeSplits.w + 0.5), 1, 3);
    float viewDepth = max(dot(fragWorldPos - camera.cameraPosition.xyz, normalize(camera.cameraForward.xyz)), 0.0);
    int cascadeIndex = 0;
    bool _959 = cascadeCount >= 3;
    bool _966;
    if (_959)
    {
        _966 = viewDepth > camera.shadowCascadeSplits.y;
    }
    else
    {
        _966 = _959;
    }
    if (_966)
    {
        cascadeIndex = 2;
    }
    else
    {
        bool _971 = cascadeCount >= 2;
        bool _978;
        if (_971)
        {
            _978 = viewDepth > camera.shadowCascadeSplits.x;
        }
        else
        {
            _978 = _971;
        }
        if (_978)
        {
            cascadeIndex = 1;
        }
    }
    vec4 lightClip = camera.shadowMatrices[cascadeIndex] * vec4(fragWorldPos, 1.0);
    if (abs(lightClip.w) <= 9.9999999747524270787835121154785e-07)
    {
        return 1.0;
    }
    vec3 lightNdc = lightClip.xyz / vec3(lightClip.w);
    vec2 shadowUv = (lightNdc.xy * 0.5) + vec2(0.5);
    float receiverDepth = lightNdc.z;
    if ((receiverDepth <= 0.0) || (receiverDepth >= 1.0))
    {
        return 1.0;
    }
    bool _1026 = shadowUv.x < 0.0;
    bool _1033;
    if (!_1026)
    {
        _1033 = shadowUv.x > 1.0;
    }
    else
    {
        _1033 = _1026;
    }
    bool _1040;
    if (!_1033)
    {
        _1040 = shadowUv.y < 0.0;
    }
    else
    {
        _1040 = _1033;
    }
    bool _1047;
    if (!_1040)
    {
        _1047 = shadowUv.y > 1.0;
    }
    else
    {
        _1047 = _1040;
    }
    if (_1047)
    {
        return 1.0;
    }
    float softness = camera.shadowParams.y;
    if (cascadeIndex == 1)
    {
        softness *= 0.64999997615814208984375;
    }
    else
    {
        if (cascadeIndex == 2)
        {
            softness *= 0.449999988079071044921875;
        }
    }
    float ndotl = clamp(dot(normal, lightDir), 0.0, 1.0);
    int param = cascadeIndex;
    float param_1 = softness;
    vec3 param_2 = normal;
    vec3 param_3 = lightDir;
    float bias = ComputeDirectionalBias(param, param_1, param_2, param_3);
    float compareDepth = clamp(receiverDepth - bias, 0.0, 1.0);
    int param_4 = cascadeIndex;
    vec3 param_5 = vec3(shadowUv, compareDepth);
    float param_6 = softness;
    float param_7 = ndotl;
    float shadow = SampleDirectionalAdaptivePCF64(param_4, param_5, param_6, param_7);
    if (cascadeIndex == (cascadeCount - 1))
    {
        float fadeWidth = clamp(camera.shadowParams.w, 0.0, 1.0);
        if (fadeWidth > 9.9999997473787516355514526367188e-05)
        {
            float fadeStart = camera.shadowCascadeSplits.z * (1.0 - fadeWidth);
            float fade = clamp((viewDepth - fadeStart) / max(camera.shadowCascadeSplits.z - fadeStart, 9.9999997473787516355514526367188e-05), 0.0, 1.0);
            shadow = mix(shadow, 1.0, fade);
        }
    }
    return shadow;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + ((vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0));
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0)) + 1.0;
    denom = (3.1415927410125732421875 * denom) * denom;
    return num / max(denom, 9.9999997473787516355514526367188e-06);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = (NdotV * (1.0 - k)) + k;
    return num / max(denom, 9.9999997473787516355514526367188e-06);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float param = NdotV;
    float param_1 = roughness;
    float ggx2 = GeometrySchlickGGX(param, param_1);
    float param_2 = NdotL;
    float param_3 = roughness;
    float ggx1 = GeometrySchlickGGX(param_2, param_3);
    return ggx1 * ggx2;
}

void main()
{
    vec3 albedo = fragBaseColor.xyz;
    float alpha = fragBaseColor.w;
    vec3 param = fragLocalNormal;
    int surfaceType = GetSurfaceType(param);
    vec3 param_1 = fragLocalPos;
    vec3 param_2 = fragLocalNormal;
    vec2 faceUV = GetFaceUV(param_1, param_2);
    int param_3 = surfaceType;
    vec2 param_4 = faceUV;
    vec3 surfaceDetail = SampleSurfaceColor(param_3, param_4);
    albedo *= surfaceDetail;
    float ignoreLighting = fragMaterial.w;
    float metalness = clamp(fragMaterial.x, 0.0, 1.0);
    float roughness = clamp(fragMaterial.y, 0.0, 1.0);
    float emissive = max(fragMaterial.z, 0.0);
    bool allowBlackNeon = camera.renderSettings.w > 0.5;
    float albedoL2 = dot(albedo, albedo);
    float glowMask = float(emissive > 0.0);
    if ((!allowBlackNeon) && (albedoL2 < 9.9999999747524270787835121154785e-07))
    {
        glowMask = 0.0;
    }
    vec3 glowBase = albedo;
    float blackNeonGlowBoost = 1.0;
    if (allowBlackNeon && (albedoL2 < 9.9999999747524270787835121154785e-07))
    {
        glowBase = vec3(0.0039215688593685626983642578125);
        blackNeonGlowBoost = 2.0;
    }
    vec3 emissiveScene = (albedo * emissive) * 4.0;
    vec3 glowColor = (((glowBase * emissive) * 8.0) * blackNeonGlowBoost) * glowMask;
    if (ignoreLighting > 0.5)
    {
        outSceneColor = vec4(albedo + emissiveScene, alpha);
        outGlowColor = vec4(0.0);
        return;
    }
    if (allowBlackNeon && (emissive > 0.0))
    {
        outSceneColor = vec4(albedo + emissiveScene, alpha);
        outGlowColor = vec4(glowColor, glowMask);
        return;
    }
    vec3 param_5 = fragLocalNormal;
    int param_6 = surfaceType;
    vec2 param_7 = faceUV;
    vec3 N = GetSurfaceMappedNormal(param_5, param_6, param_7);
    vec3 V = normalize(camera.cameraPosition.xyz - fragWorldPos);
    if (camera.renderSettings.y > 0.5)
    {
        vec3 color = camera.ambient.xyz * albedo;
        if (camera.renderSettings.x > 0.5)
        {
            vec3 L = normalize(-camera.lightDirection.xyz);
            vec3 param_8 = N;
            vec3 param_9 = L;
            float shadowFactor = ComputeShadowFactor(param_8, param_9);
            color += ((fragVertexLighting * surfaceDetail) * shadowFactor);
            float reflectionWeight = clamp(metalness, 0.0, 1.0);
            if (reflectionWeight > 9.9999997473787516355514526367188e-05)
            {
                vec3 R = reflect(-V, N);
                vec3 env = texture(skyboxTex, R).xyz * camera.skyTint.xyz;
                if (dot(env, env) < 9.9999999747524270787835121154785e-07)
                {
                    env = camera.skyTint.xyz;
                }
                vec3 F0 = mix(vec3(0.039999999105930328369140625), albedo, vec3(metalness));
                float param_10 = max(dot(N, V), 0.0);
                vec3 param_11 = F0;
                vec3 Fenv = FresnelSchlick(param_10, param_11);
                float smoothness = 1.0 - roughness;
                color += (((env * Fenv) * reflectionWeight) * (smoothness * smoothness));
            }
        }
        color += emissiveScene;
        outSceneColor = vec4(color, alpha);
        outGlowColor = vec4(glowColor, glowMask);
        return;
    }
    vec3 color_1 = camera.ambient.xyz * albedo;
    if (camera.renderSettings.x > 0.5)
    {
        vec3 L_1 = normalize(-camera.lightDirection.xyz);
        vec3 H = normalize(V + L_1);
        vec3 param_12 = N;
        vec3 param_13 = L_1;
        float shadowFactor_1 = ComputeShadowFactor(param_12, param_13);
        vec3 lightColor = camera.lightColorIntensity.xyz * camera.lightColorIntensity.w;
        float specularStrength = max(camera.lightSpecular.x, 0.0);
        float shininess = max(camera.lightSpecular.y, 1.0);
        float NdotL = max(dot(N, L_1), 0.0);
        vec3 F0_1 = mix(vec3(0.039999999105930328369140625), albedo, vec3(metalness));
        float lightShininessToRoughness = clamp(sqrt(2.0 / (shininess + 2.0)), 0.0500000007450580596923828125, 1.0);
        float effectiveRoughness = max(roughness * lightShininessToRoughness, 0.04500000178813934326171875);
        vec3 param_14 = N;
        vec3 param_15 = H;
        float param_16 = effectiveRoughness;
        float NDF = DistributionGGX(param_14, param_15, param_16);
        vec3 param_17 = N;
        vec3 param_18 = V;
        vec3 param_19 = L_1;
        float param_20 = effectiveRoughness;
        float G = GeometrySmith(param_17, param_18, param_19, param_20);
        float param_21 = max(dot(H, V), 0.0);
        vec3 param_22 = F0_1;
        vec3 F = FresnelSchlick(param_21, param_22);
        vec3 numerator = F * (NDF * G);
        float denominator = max((4.0 * max(dot(N, V), 0.0)) * NdotL, 9.9999997473787516355514526367188e-06);
        float smoothness_1 = 1.0 - roughness;
        vec3 specular = ((numerator / vec3(denominator)) * specularStrength) * (smoothness_1 * smoothness_1);
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);
        vec3 diffuse = (kD * albedo) / vec3(3.1415927410125732421875);
        color_1 += ((((diffuse + specular) * lightColor) * NdotL) * shadowFactor_1);
        float reflectionWeight_1 = clamp(metalness, 0.0, 1.0);
        if (reflectionWeight_1 > 9.9999997473787516355514526367188e-05)
        {
            vec3 R_1 = reflect(-V, N);
            vec3 env_1 = texture(skyboxTex, R_1).xyz * camera.skyTint.xyz;
            if (dot(env_1, env_1) < 9.9999999747524270787835121154785e-07)
            {
                env_1 = camera.skyTint.xyz;
            }
            float param_23 = max(dot(N, V), 0.0);
            vec3 param_24 = F0_1;
            vec3 Fenv_1 = FresnelSchlick(param_23, param_24);
            color_1 += (((env_1 * Fenv_1) * reflectionWeight_1) * (smoothness_1 * smoothness_1));
        }
    }
    color_1 += emissiveScene;
    outSceneColor = vec4(color_1, alpha);
    outGlowColor = vec4(glowColor, glowMask);
}

