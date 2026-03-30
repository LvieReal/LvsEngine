#version 450

struct DirectionalLight
{
    vec4 direction;
    vec4 shadowCascadeSplits;
    vec4 shadowParams;
    vec4 shadowBiasParams;
    vec4 shadowState;
    mat4 shadowMatrices[3];
    mat4 shadowInvMatrices[3];
};

struct InstanceData
{
    mat4 model;
    vec4 baseColor;
    vec4 material;
    vec4 surfaceData0;
    vec4 surfaceData1;
};

struct Light
{
    uint type;
    uint flags;
    uint dataIndex;
    uint shadowIndex;
    vec4 colorIntensity;
    vec4 specular;
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

layout(binding = 10, std430) readonly buffer LightsSSBO
{
    uvec4 counts;
    Light lights[64];
    DirectionalLight directionalLights[64];
} lightData;

layout(binding = 2) uniform sampler2D surfaceAtlas;
layout(binding = 15) uniform sampler2D surfaceNormalAtlas;
layout(binding = 3) uniform sampler2DShadow directionalShadowMaps[6];
layout(binding = 14) uniform sampler3D directionalShadowJitter;
layout(binding = 13) uniform sampler2D neonTexture;
layout(binding = 1) uniform samplerCube skyboxTex;

layout(location = 7) flat in uint fragInstanceIndex;
layout(location = 2) in vec4 fragBaseColor;
layout(location = 6) in vec3 fragLocalNormal;
layout(location = 5) in vec3 fragLocalPos;
layout(location = 3) in vec4 fragMaterial;
layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outGlowColor;
layout(location = 2) out vec4 outDepthColor;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 4) in vec3 fragVertexLighting;
layout(location = 0) in vec3 fragNormal;
InstanceData inst;

int GetTopSurfaceType()
{
    return int(inst.surfaceData0.x + 0.5);
}

int GetBottomSurfaceType()
{
    return int(inst.surfaceData0.y + 0.5);
}

int GetRightSurfaceType()
{
    return int(inst.surfaceData1.y + 0.5);
}

int GetLeftSurfaceType()
{
    return int(inst.surfaceData1.x + 0.5);
}

int GetFrontSurfaceType()
{
    return int(inst.surfaceData0.z + 0.5);
}

int GetBackSurfaceType()
{
    return int(inst.surfaceData0.w + 0.5);
}

int GetSurfaceType(vec3 normal)
{
    vec3 n = normalize(normal);
    vec3 a = abs(n);
    bool _529 = a.y >= a.x;
    bool _537;
    if (_529)
    {
        _537 = a.y >= a.z;
    }
    else
    {
        _537 = _529;
    }
    if (_537)
    {
        int _543;
        if (n.y > 0.0)
        {
            _543 = GetTopSurfaceType();
        }
        else
        {
            _543 = GetBottomSurfaceType();
        }
        return _543;
    }
    bool _555 = a.x >= a.y;
    bool _563;
    if (_555)
    {
        _563 = a.x >= a.z;
    }
    else
    {
        _563 = _555;
    }
    if (_563)
    {
        int _569;
        if (n.x > 0.0)
        {
            _569 = GetRightSurfaceType();
        }
        else
        {
            _569 = GetLeftSurfaceType();
        }
        return _569;
    }
    int _580;
    if (n.z > 0.0)
    {
        _580 = GetFrontSurfaceType();
    }
    else
    {
        _580 = GetBackSurfaceType();
    }
    return _580;
}

vec3 GetMeshSizeFromModel()
{
    vec3 xAxis = vec3(inst.model[0].x, inst.model[0].y, inst.model[0].z);
    vec3 yAxis = vec3(inst.model[1].x, inst.model[1].y, inst.model[1].z);
    vec3 zAxis = vec3(inst.model[2].x, inst.model[2].y, inst.model[2].z);
    return vec3(length(xAxis), length(yAxis), length(zAxis));
}

vec2 GetFaceUV(vec3 localPos, vec3 localNormal)
{
    vec3 size = GetMeshSizeFromModel();
    vec3 scaledPos = localPos * size;
    vec3 halfSize = size * 0.5;
    vec3 n = normalize(localNormal);
    vec3 a = abs(n);
    bool _608 = a.x >= a.y;
    bool _616;
    if (_608)
    {
        _616 = a.x >= a.z;
    }
    else
    {
        _616 = _608;
    }
    vec2 uv;
    if (_616)
    {
        uv = scaledPos.zy + halfSize.zy;
    }
    else
    {
        bool _630 = a.y >= a.x;
        bool _638;
        if (_630)
        {
            _638 = a.y >= a.z;
        }
        else
        {
            _638 = _630;
        }
        if (_638)
        {
            uv = scaledPos.xz + halfSize.xz;
        }
        else
        {
            uv = scaledPos.xy + halfSize.xy;
        }
    }
    return fract(uv * vec2(0.5));
}

bool IsSurfaceEnabled()
{
    return inst.surfaceData1.z > 0.5;
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
    bool _734 = surfaceType == 0;
    bool _740;
    if (!_734)
    {
        _740 = !IsSurfaceEnabled();
    }
    else
    {
        _740 = _734;
    }
    if (_740)
    {
        return vec3(1.0);
    }
    int param = surfaceType;
    vec2 param_1 = uv;
    return texture(surfaceAtlas, GetSurfaceAtlasUV(param, param_1)).xyz;
}

bool IsSurfaceNormalEnabled()
{
    return inst.surfaceData1.w > 0.5;
}

vec3 GetSurfaceMappedNormal(vec3 localNormal, int surfaceType, vec2 uv)
{
    bool _756 = !IsSurfaceEnabled();
    bool _762;
    if (!_756)
    {
        _762 = !IsSurfaceNormalEnabled();
    }
    else
    {
        _762 = _756;
    }
    if (_762 || (surfaceType == 0))
    {
        mat3 normalMat = transpose(inverse(mat3(inst.model[0].xyz, inst.model[1].xyz, inst.model[2].xyz)));
        return normalize(normalMat * localNormal);
    }
    vec3 n = normalize(localNormal);
    vec3 a = abs(n);
    bool _798 = a.x >= a.y;
    bool _806;
    if (_798)
    {
        _806 = a.x >= a.z;
    }
    else
    {
        _806 = _798;
    }
    vec3 tangentLocal;
    vec3 bitangentLocal;
    if (_806)
    {
        tangentLocal = vec3(0.0, 0.0, 1.0);
        bitangentLocal = vec3(0.0, 1.0, 0.0);
    }
    else
    {
        bool _818 = a.y >= a.x;
        bool _826;
        if (_818)
        {
            _826 = a.y >= a.z;
        }
        else
        {
            _826 = _818;
        }
        if (_826)
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
    mat3 modelMat = mat3(inst.model[0].xyz, inst.model[1].xyz, inst.model[2].xyz);
    mat3 normalMat_1 = transpose(inverse(modelMat));
    vec3 normalWorld = normalize(normalMat_1 * n);
    vec3 tangentWorld = normalize(modelMat * tangentLocal);
    vec3 bitangentWorld = normalize(cross(normalWorld, tangentWorld));
    tangentWorld = normalize(cross(bitangentWorld, normalWorld));
    vec3 mappedWorld = normalize(((tangentWorld * tangentSpaceNormal.x) + (bitangentWorld * tangentSpaceNormal.y)) + (normalWorld * tangentSpaceNormal.z));
    return mappedWorld;
}

vec2 GetDirectionalShadowTexelSize(int shadowMapBase, int cascadeIndex)
{
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    ivec2 size = max(textureSize(directionalShadowMaps[idx], 0), ivec2(1));
    return vec2(1.0) / vec2(size);
}

vec3 ApplyDirectionalNormalOffset(DirectionalLight dl, int cascadeIndex, vec3 worldPos, vec3 normal, vec2 shadowUv, float receiverDepth, vec2 texelSize)
{
    float normalOffsetTexels = max(dl.shadowBiasParams.x, 0.0);
    if (normalOffsetTexels <= 9.9999999747524270787835121154785e-07)
    {
        return worldPos;
    }
    vec3 ndc0 = vec3((shadowUv * 2.0) - vec2(1.0), receiverDepth);
    vec3 ndcX = vec3(((shadowUv + vec2(texelSize.x, 0.0)) * 2.0) - vec2(1.0), receiverDepth);
    vec3 ndcY = vec3(((shadowUv + vec2(0.0, texelSize.y)) * 2.0) - vec2(1.0), receiverDepth);
    vec4 w0 = dl.shadowInvMatrices[cascadeIndex] * vec4(ndc0, 1.0);
    vec4 wX = dl.shadowInvMatrices[cascadeIndex] * vec4(ndcX, 1.0);
    vec4 wY = dl.shadowInvMatrices[cascadeIndex] * vec4(ndcY, 1.0);
    float invW0 = 1.0 / max(abs(w0.w), 9.9999999747524270787835121154785e-07);
    float invWX = 1.0 / max(abs(wX.w), 9.9999999747524270787835121154785e-07);
    float invWY = 1.0 / max(abs(wY.w), 9.9999999747524270787835121154785e-07);
    vec3 p0 = w0.xyz * invW0;
    vec3 pX = wX.xyz * invWX;
    vec3 pY = wY.xyz * invWY;
    float worldPerTexel = max(length(pX - p0), length(pY - p0));
    float offsetWorld = normalOffsetTexels * worldPerTexel;
    return worldPos + (normalize(normal) * offsetWorld);
}

float ComputeDirectionalBias(DirectionalLight dl, int shadowMapBase, int cascadeIndex, vec3 normal, vec3 lightDir)
{
    int param = shadowMapBase;
    int param_1 = cascadeIndex;
    vec2 texelSize = GetDirectionalShadowTexelSize(param, param_1);
    float texelScale = max(texelSize.x, texelSize.y);
    float depthBiasTexels = max(dl.shadowParams.x, 0.0);
    return depthBiasTexels * texelScale;
}

ivec2 GetDirectionalShadowMapSize(int shadowMapBase, int cascadeIndex)
{
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    return max(textureSize(directionalShadowMaps[idx], 0), ivec2(1));
}

float SampleDirectionalShadowMap(int shadowMapBase, int cascadeIndex, vec3 shadowCoord)
{
    int idx = clamp(shadowMapBase + cascadeIndex, 0, 5);
    return texture(directionalShadowMaps[idx], vec3(shadowCoord.xy, shadowCoord.z));
}

float SampleDirectionalShadowHard(int shadowMapBase, int cascadeIndex, vec2 uv, float compareDepth)
{
    vec2 cuv = clamp(uv, vec2(0.0), vec2(1.0));
    int param = shadowMapBase;
    int param_1 = cascadeIndex;
    vec3 param_2 = vec3(cuv, compareDepth);
    return float(SampleDirectionalShadowMap(param, param_1, param_2) > 0.5);
}

float ShadowTestRBSM(int shadowMapBase, int cascadeIndex, vec3 p)
{
    int param = shadowMapBase;
    int param_1 = cascadeIndex;
    vec2 param_2 = p.xy;
    float param_3 = p.z;
    return SampleDirectionalShadowHard(param, param_1, param_2, param_3);
}

vec4 ComputeDiscontinuity4_RBSM(int shadowMapBase, int cascadeIndex, vec3 p, vec2 o)
{
    int param = shadowMapBase;
    int param_1 = cascadeIndex;
    vec3 param_2 = p;
    float center = ShadowTestRBSM(param, param_1, param_2);
    int param_3 = shadowMapBase;
    int param_4 = cascadeIndex;
    vec3 param_5 = vec3(p.x - o.x, p.y, p.z);
    float left = ShadowTestRBSM(param_3, param_4, param_5);
    int param_6 = shadowMapBase;
    int param_7 = cascadeIndex;
    vec3 param_8 = vec3(p.x + o.x, p.y, p.z);
    float right = ShadowTestRBSM(param_6, param_7, param_8);
    int param_9 = shadowMapBase;
    int param_10 = cascadeIndex;
    vec3 param_11 = vec3(p.x, p.y - o.y, p.z);
    float bottom = ShadowTestRBSM(param_9, param_10, param_11);
    int param_12 = shadowMapBase;
    int param_13 = cascadeIndex;
    vec3 param_14 = vec3(p.x, p.y + o.y, p.z);
    float top = ShadowTestRBSM(param_12, param_13, param_14);
    return abs(vec4(left, right, bottom, top) - vec4(center));
}

float ComputeRelativeDistance1_RBSM(int shadowMapBase, int cascadeIndex, vec3 p, vec2 dir, float c, vec2 o)
{
    vec3 np = p;
    float foundSilhouetteEnd = 0.0;
    float _distance = 0.0;
    vec2 stepUv = dir * o;
    for (int it = 0; it < 16; it++)
    {
        vec3 _1092 = np;
        vec2 _1094 = _1092.xy + stepUv;
        np.x = _1094.x;
        np.y = _1094.y;
        bool _1101 = np.x < 0.0;
        bool _1108;
        if (!_1101)
        {
            _1108 = np.x > 1.0;
        }
        else
        {
            _1108 = _1101;
        }
        bool _1115;
        if (!_1108)
        {
            _1115 = np.y < 0.0;
        }
        else
        {
            _1115 = _1108;
        }
        bool _1122;
        if (!_1115)
        {
            _1122 = np.y > 1.0;
        }
        else
        {
            _1122 = _1115;
        }
        if (_1122)
        {
            break;
        }
        int param = shadowMapBase;
        int param_1 = cascadeIndex;
        vec3 param_2 = np;
        float center = ShadowTestRBSM(param, param_1, param_2);
        bool isCenterUmbra = center < 0.5;
        if (isCenterUmbra)
        {
            foundSilhouetteEnd = 1.0;
            break;
        }
        int param_3 = shadowMapBase;
        int param_4 = cascadeIndex;
        vec3 param_5 = np;
        vec2 param_6 = o;
        vec4 d = ComputeDiscontinuity4_RBSM(param_3, param_4, param_5, param_6);
        if ((((d.x + d.y) + d.z) + d.w) == 0.0)
        {
            break;
        }
        _distance += 1.0;
    }
    _distance += (1.0 - c);
    return mix(-_distance, _distance, foundSilhouetteEnd);
}

vec4 ComputeRelativeDistance4_RBSM(int shadowMapBase, int cascadeIndex, vec3 p, vec2 c, vec2 o)
{
    int param = shadowMapBase;
    int param_1 = cascadeIndex;
    vec3 param_2 = p;
    vec2 param_3 = vec2(-1.0, 0.0);
    float param_4 = 1.0 - c.x;
    vec2 param_5 = o;
    float dl = ComputeRelativeDistance1_RBSM(param, param_1, param_2, param_3, param_4, param_5);
    int param_6 = shadowMapBase;
    int param_7 = cascadeIndex;
    vec3 param_8 = p;
    vec2 param_9 = vec2(1.0, 0.0);
    float param_10 = c.x;
    vec2 param_11 = o;
    float dr = ComputeRelativeDistance1_RBSM(param_6, param_7, param_8, param_9, param_10, param_11);
    vec2 bottomDir = vec2(0.0, -1.0);
    vec2 topDir = vec2(0.0, 1.0);
    int param_12 = shadowMapBase;
    int param_13 = cascadeIndex;
    vec3 param_14 = p;
    vec2 param_15 = bottomDir;
    float param_16 = 1.0 - c.y;
    vec2 param_17 = o;
    float db = ComputeRelativeDistance1_RBSM(param_12, param_13, param_14, param_15, param_16, param_17);
    int param_18 = shadowMapBase;
    int param_19 = cascadeIndex;
    vec3 param_20 = p;
    vec2 param_21 = topDir;
    float param_22 = c.y;
    vec2 param_23 = o;
    float dt = ComputeRelativeDistance1_RBSM(param_18, param_19, param_20, param_21, param_22, param_23);
    return vec4(dl, dr, db, dt);
}

float NormalizeRelativeDistance1_RBSM(vec2 dist)
{
    float T = 1.0;
    bool _1259 = dist.x < 0.0;
    bool _1265;
    if (_1259)
    {
        _1265 = dist.y < 0.0;
    }
    else
    {
        _1265 = _1259;
    }
    if (_1265)
    {
        T = 0.0;
    }
    bool _1270 = dist.x > 0.0;
    bool _1276;
    if (_1270)
    {
        _1276 = dist.y > 0.0;
    }
    else
    {
        _1276 = _1270;
    }
    if (_1276)
    {
        T = -2.0;
    }
    float len = min(abs(dist.x) + abs(dist.y), 16.0);
    float _1293;
    if (len > 9.9999999747524270787835121154785e-07)
    {
        _1293 = abs(max(T * dist.x, T * dist.y)) / len;
    }
    else
    {
        _1293 = 0.0;
    }
    return _1293;
}

vec2 NormalizeRelativeDistance2_RBSM(vec4 dist)
{
    vec2 param = vec2(dist.x, dist.y);
    vec2 param_1 = vec2(dist.z, dist.w);
    return vec2(NormalizeRelativeDistance1_RBSM(param), NormalizeRelativeDistance1_RBSM(param_1));
}

float RevectorizeShadow_RBSM(vec2 r)
{
    bool _1334 = (r.x * r.y) > 0.0;
    bool _1343;
    if (_1334)
    {
        _1343 = (1.0 - r.x) > r.y;
    }
    else
    {
        _1343 = _1334;
    }
    if (_1343)
    {
        return 0.0;
    }
    return 1.0;
}

float SampleDirectionalRBSM(int shadowMapBase, int cascadeIndex, vec2 shadowUv, float compareDepth)
{
    int param = shadowMapBase;
    int param_1 = cascadeIndex;
    ivec2 shadowSize = GetDirectionalShadowMapSize(param, param_1);
    vec2 o = vec2(1.0) / vec2(shadowSize);
    vec3 p = vec3(shadowUv, compareDepth);
    int param_2 = shadowMapBase;
    int param_3 = cascadeIndex;
    vec3 param_4 = p;
    float shadowTestResult = ShadowTestRBSM(param_2, param_3, param_4);
    if (shadowTestResult < 0.5)
    {
        return 0.0;
    }
    vec2 c = fract(shadowUv * vec2(shadowSize));
    int param_5 = shadowMapBase;
    int param_6 = cascadeIndex;
    vec3 param_7 = p;
    vec2 param_8 = o;
    vec4 disc = ComputeDiscontinuity4_RBSM(param_5, param_6, param_7, param_8);
    if ((((disc.x + disc.y) + disc.z) + disc.w) > 0.0)
    {
        int param_9 = shadowMapBase;
        int param_10 = cascadeIndex;
        vec3 param_11 = p;
        vec2 param_12 = c;
        vec2 param_13 = o;
        vec4 dist = ComputeRelativeDistance4_RBSM(param_9, param_10, param_11, param_12, param_13);
        vec4 param_14 = dist;
        vec2 r = NormalizeRelativeDistance2_RBSM(param_14);
        vec2 param_15 = r;
        return RevectorizeShadow_RBSM(param_15);
    }
    return shadowTestResult;
}

float SampleDirectionalPCF(DirectionalLight dl, int shadowMapBase, int cascadeIndex, vec3 baseShadowCoord, float radiusTexels, vec3 normal, vec3 lightDir, vec2 fragCoord)
{
    int desiredTaps = clamp(int(dl.shadowParams.z + 0.5), 1, 64);
    if (desiredTaps <= 1)
    {
        int param = shadowMapBase;
        int param_1 = cascadeIndex;
        vec3 param_2 = baseShadowCoord;
        return SampleDirectionalShadowMap(param, param_1, param_2);
    }
    int param_3 = shadowMapBase;
    int param_4 = cascadeIndex;
    vec2 texelSize = GetDirectionalShadowTexelSize(param_3, param_4);
    float radius = max(radiusTexels, 0.0);
    float ndotl = max(dot(normalize(normal), normalize(lightDir)), 0.0);
    vec3 jcoord = vec3(fragCoord * dl.shadowState.yz, 0.0);
    float shadowSum = 0.0;
    int pretestTaps = min(desiredTaps, 8);
    int i = 0;
    for (;;)
    {
        bool _1489 = i < 32;
        bool _1496;
        if (_1489)
        {
            _1496 = (i * 2) < pretestTaps;
        }
        else
        {
            _1496 = _1489;
        }
        if (_1496)
        {
            vec4 offset = (texture(directionalShadowJitter, jcoord) * 2.0) - vec4(1.0);
            jcoord.z += 0.03125;
            vec3 smCoord = baseShadowCoord;
            vec2 uvOffset = (offset.xy * texelSize) * radius;
            vec2 _1525 = uvOffset + baseShadowCoord.xy;
            smCoord.x = _1525.x;
            smCoord.y = _1525.y;
            int param_5 = shadowMapBase;
            int param_6 = cascadeIndex;
            vec3 param_7 = smCoord;
            shadowSum += SampleDirectionalShadowMap(param_5, param_6, param_7);
            if (((i * 2) + 1) < pretestTaps)
            {
                uvOffset = (offset.zw * texelSize) * radius;
                vec2 _1555 = uvOffset + baseShadowCoord.xy;
                smCoord.x = _1555.x;
                smCoord.y = _1555.y;
                int param_8 = shadowMapBase;
                int param_9 = cascadeIndex;
                vec3 param_10 = smCoord;
                shadowSum += SampleDirectionalShadowMap(param_8, param_9, param_10);
            }
            i++;
            continue;
        }
        else
        {
            break;
        }
    }
    float shadowPretest = shadowSum / float(max(pretestTaps, 1));
    if (desiredTaps <= pretestTaps)
    {
        return shadowPretest;
    }
    if (((ndotl > 0.0) && (shadowPretest > 0.0)) && (shadowPretest < 1.0))
    {
        int tapsDone = pretestTaps;
        for (int i_1 = 0; (i_1 < 32) && (tapsDone < desiredTaps); i_1++)
        {
            vec4 offset_1 = (texture(directionalShadowJitter, jcoord) * 2.0) - vec4(1.0);
            jcoord.z += 0.03125;
            vec3 smCoord_1 = baseShadowCoord;
            vec2 uvOffset_1 = (offset_1.xy * texelSize) * radius;
            vec2 _1631 = uvOffset_1 + baseShadowCoord.xy;
            smCoord_1.x = _1631.x;
            smCoord_1.y = _1631.y;
            int param_11 = shadowMapBase;
            int param_12 = cascadeIndex;
            vec3 param_13 = smCoord_1;
            shadowSum += SampleDirectionalShadowMap(param_11, param_12, param_13);
            tapsDone++;
            if (tapsDone < desiredTaps)
            {
                uvOffset_1 = (offset_1.zw * texelSize) * radius;
                vec2 _1661 = uvOffset_1 + baseShadowCoord.xy;
                smCoord_1.x = _1661.x;
                smCoord_1.y = _1661.y;
                int param_14 = shadowMapBase;
                int param_15 = cascadeIndex;
                vec3 param_16 = smCoord_1;
                shadowSum += SampleDirectionalShadowMap(param_14, param_15, param_16);
                tapsDone++;
            }
        }
        return shadowSum / float(desiredTaps);
    }
    return shadowPretest;
}

float ComputeDirectionalShadowFactor(DirectionalLight dl, int shadowMapBase, vec3 normal, vec3 lightDir, vec3 worldPos, vec3 cameraPos, vec3 cameraForward, vec2 fragCoord)
{
    if (dl.shadowState.x < 0.5)
    {
        return 1.0;
    }
    int cascadeCount = clamp(int(dl.shadowCascadeSplits.w + 0.5), 1, 3);
    vec3 fwd = normalize(cameraForward);
    float viewDepth = max(dot(worldPos - cameraPos, fwd), 0.0);
    int cascadeIndex = 0;
    bool _1864 = cascadeCount >= 3;
    bool _1871;
    if (_1864)
    {
        _1871 = viewDepth > dl.shadowCascadeSplits.y;
    }
    else
    {
        _1871 = _1864;
    }
    if (_1871)
    {
        cascadeIndex = 2;
    }
    else
    {
        bool _1876 = cascadeCount >= 2;
        bool _1883;
        if (_1876)
        {
            _1883 = viewDepth > dl.shadowCascadeSplits.x;
        }
        else
        {
            _1883 = _1876;
        }
        if (_1883)
        {
            cascadeIndex = 1;
        }
    }
    vec4 lightClip = dl.shadowMatrices[cascadeIndex] * vec4(worldPos, 1.0);
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
    bool _1929 = shadowUv.x < 0.0;
    bool _1936;
    if (!_1929)
    {
        _1936 = shadowUv.x > 1.0;
    }
    else
    {
        _1936 = _1929;
    }
    bool _1943;
    if (!_1936)
    {
        _1943 = shadowUv.y < 0.0;
    }
    else
    {
        _1943 = _1936;
    }
    bool _1950;
    if (!_1943)
    {
        _1950 = shadowUv.y > 1.0;
    }
    else
    {
        _1950 = _1943;
    }
    if (_1950)
    {
        return 1.0;
    }
    int param = shadowMapBase;
    int param_1 = cascadeIndex;
    vec2 texelSize = GetDirectionalShadowTexelSize(param, param_1);
    DirectionalLight param_2 = dl;
    int param_3 = cascadeIndex;
    vec3 param_4 = worldPos;
    vec3 param_5 = normal;
    vec2 param_6 = shadowUv;
    float param_7 = receiverDepth;
    vec2 param_8 = texelSize;
    vec3 biasedWorldPos = ApplyDirectionalNormalOffset(param_2, param_3, param_4, param_5, param_6, param_7, param_8);
    if (distance(biasedWorldPos, worldPos) > 1.0000000116860974230803549289703e-07)
    {
        lightClip = dl.shadowMatrices[cascadeIndex] * vec4(biasedWorldPos, 1.0);
        if (abs(lightClip.w) <= 9.9999999747524270787835121154785e-07)
        {
            return 1.0;
        }
        lightNdc = lightClip.xyz / vec3(lightClip.w);
        shadowUv = (lightNdc.xy * 0.5) + vec2(0.5);
        receiverDepth = lightNdc.z;
        if ((receiverDepth <= 0.0) || (receiverDepth >= 1.0))
        {
            return 1.0;
        }
        bool _2022 = shadowUv.x < 0.0;
        bool _2029;
        if (!_2022)
        {
            _2029 = shadowUv.x > 1.0;
        }
        else
        {
            _2029 = _2022;
        }
        bool _2036;
        if (!_2029)
        {
            _2036 = shadowUv.y < 0.0;
        }
        else
        {
            _2036 = _2029;
        }
        bool _2043;
        if (!_2036)
        {
            _2043 = shadowUv.y > 1.0;
        }
        else
        {
            _2043 = _2036;
        }
        if (_2043)
        {
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
    DirectionalLight param_9 = dl;
    int param_10 = shadowMapBase;
    int param_11 = cascadeIndex;
    vec3 param_12 = normal;
    vec3 param_13 = lightDir;
    float bias = ComputeDirectionalBias(param_9, param_10, param_11, param_12, param_13);
    float compareDepth = clamp(receiverDepth - bias, 0.0, 1.0);
    float shadow = 1.0;
    if (dl.shadowState.w > 0.5)
    {
        int param_14 = shadowMapBase;
        int param_15 = cascadeIndex;
        vec2 param_16 = shadowUv;
        float param_17 = compareDepth;
        shadow = SampleDirectionalRBSM(param_14, param_15, param_16, param_17);
    }
    else
    {
        DirectionalLight param_18 = dl;
        int param_19 = shadowMapBase;
        int param_20 = cascadeIndex;
        vec3 param_21 = vec3(shadowUv, compareDepth);
        float param_22 = softness;
        vec3 param_23 = normal;
        vec3 param_24 = lightDir;
        vec2 param_25 = fragCoord;
        shadow = SampleDirectionalPCF(param_18, param_19, param_20, param_21, param_22, param_23, param_24, param_25);
    }
    if (cascadeIndex == (cascadeCount - 1))
    {
        float fadeWidth = clamp(dl.shadowParams.w, 0.0, 1.0);
        if (fadeWidth > 9.9999997473787516355514526367188e-05)
        {
            float fadeStart = dl.shadowCascadeSplits.z * (1.0 - fadeWidth);
            float fade = clamp((viewDepth - fadeStart) / max(dl.shadowCascadeSplits.z - fadeStart, 9.9999997473787516355514526367188e-05), 0.0, 1.0);
            shadow = mix(shadow, 1.0, fade);
        }
    }
    return shadow;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + ((vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0));
}

uint GetSpecularHighlightType(vec4 lightSpecular)
{
    uint t = uint(lightSpecular.w + 0.5);
    if ((t < 1u) || (t > 3u))
    {
        t = 3u;
    }
    return t;
}

vec3 SpecularPhong(vec3 N, vec3 V, vec3 L, vec3 F, float shininess)
{
    vec3 R = reflect(-L, N);
    float s = pow(max(dot(R, V), 0.0), shininess);
    return F * s;
}

vec3 SpecularBlinnPhong(vec3 N, vec3 H, vec3 F, float shininess)
{
    float s = pow(max(dot(N, H), 0.0), shininess);
    return F * s;
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

vec3 SpecularCookTorrance(vec3 N, vec3 V, vec3 L, vec3 H, vec3 F, float effectiveRoughness)
{
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    vec3 param = N;
    vec3 param_1 = H;
    float param_2 = effectiveRoughness;
    float D = DistributionGGX(param, param_1, param_2);
    vec3 param_3 = N;
    vec3 param_4 = V;
    vec3 param_5 = L;
    float param_6 = effectiveRoughness;
    float G = GeometrySmith(param_3, param_4, param_5, param_6);
    vec3 numerator = F * (D * G);
    float denominator = max((4.0 * NdotV) * NdotL, 9.9999997473787516355514526367188e-06);
    return numerator / vec3(denominator);
}

void main()
{
    inst.model = instanceData.instances[fragInstanceIndex].model;
    inst.baseColor = instanceData.instances[fragInstanceIndex].baseColor;
    inst.material = instanceData.instances[fragInstanceIndex].material;
    inst.surfaceData0 = instanceData.instances[fragInstanceIndex].surfaceData0;
    inst.surfaceData1 = instanceData.instances[fragInstanceIndex].surfaceData1;
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
    uint materialFlags = uint(fragMaterial.w + 0.5);
    bool ignoreLighting = (materialFlags & 1u) != 0u;
    bool excludeHbao = (materialFlags & 2u) != 0u;
    float _2253;
    if (excludeHbao)
    {
        _2253 = 1.0;
    }
    else
    {
        _2253 = gl_FragCoord.z;
    }
    float hbaoDepth = _2253;
    float metalness = clamp(fragMaterial.x, 0.0, 1.0);
    float roughness = clamp(fragMaterial.y, 0.0, 1.0);
    float emissive = max(fragMaterial.z, 0.0);
    bool neonEnabled = camera.renderSettings.z > 0.5;
    bool allowBlackNeon = (camera.renderSettings.w > 0.5) && neonEnabled;
    float albedoL2 = dot(albedo, albedo);
    float glowMask = float((neonEnabled && (!ignoreLighting)) && (emissive > 9.9999997473787516355514526367188e-05));
    if ((!allowBlackNeon) && (albedoL2 < 9.9999999747524270787835121154785e-07))
    {
        glowMask = 0.0;
    }
    float _2309;
    if (allowBlackNeon)
    {
        _2309 = 0.0;
    }
    else
    {
        _2309 = smoothstep(1.0, 8.0, emissive);
    }
    float whiteShift = _2309;
    vec3 emissionRaw = albedo * emissive;
    float emissionMax = max(emissionRaw.x, max(emissionRaw.y, emissionRaw.z));
    vec3 emissionChroma = emissionRaw / vec3(max(emissionMax, 9.9999999747524270787835121154785e-07));
    vec3 emissionTint = mix(emissionChroma, vec3(1.0), vec3(whiteShift));
    vec3 emissiveScene = emissionTint * emissionMax;
    vec3 glowBase = albedo;
    if (allowBlackNeon && (albedoL2 < 9.9999999747524270787835121154785e-07))
    {
        glowBase = vec3(0.0039215688593685626983642578125);
    }
    vec3 glowRaw = glowBase * emissive;
    float glowMax = max(glowRaw.x, max(glowRaw.y, glowRaw.z));
    vec3 glowChroma = glowRaw / vec3(max(glowMax, 9.9999999747524270787835121154785e-07));
    vec3 glowTint = mix(glowChroma, vec3(1.0), vec3(whiteShift));
    vec3 glowColor = (glowTint * glowMax) * glowMask;
    vec2 neonUv = gl_FragCoord.xy / vec2(max(textureSize(neonTexture, 0), ivec2(1)));
    vec3 _2396;
    if (neonEnabled)
    {
        _2396 = texture(neonTexture, neonUv).xyz;
    }
    else
    {
        _2396 = vec3(0.0);
    }
    vec3 neonSample = _2396;
    if (ignoreLighting)
    {
        outSceneColor = vec4((albedo + emissiveScene) + ((neonSample * 0.100000001490116119384765625) * glowMask), alpha);
        outGlowColor = vec4(0.0);
        outDepthColor = vec4(hbaoDepth, 0.0, 0.0, 1.0);
        return;
    }
    if (emissive > 9.9999997473787516355514526367188e-05)
    {
        outSceneColor = vec4((albedo + emissiveScene) + ((neonSample * 0.100000001490116119384765625) * glowMask), alpha);
        outGlowColor = vec4(glowColor, glowMask);
        outDepthColor = vec4(hbaoDepth, 0.0, 0.0, 1.0);
        return;
    }
    vec3 param_5 = fragLocalNormal;
    int param_6 = surfaceType;
    vec2 param_7 = faceUV;
    vec3 N = GetSurfaceMappedNormal(param_5, param_6, param_7);
    vec3 V = normalize(camera.cameraPosition.xyz - fragWorldPos);
    if (camera.lightingSettings.x > 0.5)
    {
        vec3 color = (camera.ambient.xyz * camera.ambient.w) * albedo;
        float shadowFactor = 1.0;
        float fresnelAmount = 1.0;
        uint lightCount = min(lightData.counts.x, 64u);
        Light light;
        DirectionalLight dl;
        for (uint i = 0u; i < lightCount; i++)
        {
            light.type = lightData.lights[i].type;
            light.flags = lightData.lights[i].flags;
            light.dataIndex = lightData.lights[i].dataIndex;
            light.shadowIndex = lightData.lights[i].shadowIndex;
            light.colorIntensity = lightData.lights[i].colorIntensity;
            light.specular = lightData.lights[i].specular;
            if ((light.flags & 1u) == 0u)
            {
                continue;
            }
            fresnelAmount = clamp(light.specular.z, 0.0, 1.0);
            bool _2546 = light.type == 0u;
            bool _2553;
            if (_2546)
            {
                _2553 = light.shadowIndex != 4294967295u;
            }
            else
            {
                _2553 = _2546;
            }
            if (_2553)
            {
                dl.direction = lightData.directionalLights[light.dataIndex].direction;
                dl.shadowCascadeSplits = lightData.directionalLights[light.dataIndex].shadowCascadeSplits;
                dl.shadowParams = lightData.directionalLights[light.dataIndex].shadowParams;
                dl.shadowBiasParams = lightData.directionalLights[light.dataIndex].shadowBiasParams;
                dl.shadowState = lightData.directionalLights[light.dataIndex].shadowState;
                dl.shadowMatrices[0] = lightData.directionalLights[light.dataIndex].shadowMatrices[0];
                dl.shadowMatrices[1] = lightData.directionalLights[light.dataIndex].shadowMatrices[1];
                dl.shadowMatrices[2] = lightData.directionalLights[light.dataIndex].shadowMatrices[2];
                dl.shadowInvMatrices[0] = lightData.directionalLights[light.dataIndex].shadowInvMatrices[0];
                dl.shadowInvMatrices[1] = lightData.directionalLights[light.dataIndex].shadowInvMatrices[1];
                dl.shadowInvMatrices[2] = lightData.directionalLights[light.dataIndex].shadowInvMatrices[2];
                vec3 L = normalize(-dl.direction.xyz);
                int shadowMapBase = int(light.shadowIndex) * 3;
                DirectionalLight param_8 = dl;
                int param_9 = shadowMapBase;
                vec3 param_10 = N;
                vec3 param_11 = L;
                vec3 param_12 = fragWorldPos;
                vec3 param_13 = camera.cameraPosition.xyz;
                vec3 param_14 = camera.cameraForward.xyz;
                vec2 param_15 = gl_FragCoord.xy;
                shadowFactor = ComputeDirectionalShadowFactor(param_8, param_9, param_10, param_11, param_12, param_13, param_14, param_15);
                break;
            }
        }
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
            float param_16 = max(dot(N, V), 0.0);
            vec3 param_17 = F0;
            vec3 FenvSchlick = FresnelSchlick(param_16, param_17);
            vec3 Fenv = mix(F0, FenvSchlick, vec3(fresnelAmount));
            float smoothness = 1.0 - roughness;
            color += (((env * Fenv) * reflectionWeight) * (smoothness * smoothness));
        }
        color += emissiveScene;
        outSceneColor = vec4(color + ((neonSample * 0.100000001490116119384765625) * glowMask), alpha);
        outGlowColor = vec4(glowColor, glowMask);
        outDepthColor = vec4(hbaoDepth, 0.0, 0.0, 1.0);
        return;
    }
    vec3 color_1 = (camera.ambient.xyz * camera.ambient.w) * albedo;
    float fresnelAmountEnv = 1.0;
    uint lightCount_1 = min(lightData.counts.x, 64u);
    Light light_1;
    for (uint i_1 = 0u; i_1 < lightCount_1; i_1++)
    {
        light_1.type = lightData.lights[i_1].type;
        light_1.flags = lightData.lights[i_1].flags;
        light_1.dataIndex = lightData.lights[i_1].dataIndex;
        light_1.shadowIndex = lightData.lights[i_1].shadowIndex;
        light_1.colorIntensity = lightData.lights[i_1].colorIntensity;
        light_1.specular = lightData.lights[i_1].specular;
        if ((light_1.flags & 1u) == 0u)
        {
            continue;
        }
        fresnelAmountEnv = clamp(light_1.specular.z, 0.0, 1.0);
        break;
    }
    Light light_2;
    DirectionalLight dl_1;
    for (uint i_2 = 0u; i_2 < lightCount_1; i_2++)
    {
        light_2.type = lightData.lights[i_2].type;
        light_2.flags = lightData.lights[i_2].flags;
        light_2.dataIndex = lightData.lights[i_2].dataIndex;
        light_2.shadowIndex = lightData.lights[i_2].shadowIndex;
        light_2.colorIntensity = lightData.lights[i_2].colorIntensity;
        light_2.specular = lightData.lights[i_2].specular;
        if ((light_2.flags & 1u) == 0u)
        {
            continue;
        }
        if (light_2.type == 0u)
        {
            dl_1.direction = lightData.directionalLights[light_2.dataIndex].direction;
            dl_1.shadowCascadeSplits = lightData.directionalLights[light_2.dataIndex].shadowCascadeSplits;
            dl_1.shadowParams = lightData.directionalLights[light_2.dataIndex].shadowParams;
            dl_1.shadowBiasParams = lightData.directionalLights[light_2.dataIndex].shadowBiasParams;
            dl_1.shadowState = lightData.directionalLights[light_2.dataIndex].shadowState;
            dl_1.shadowMatrices[0] = lightData.directionalLights[light_2.dataIndex].shadowMatrices[0];
            dl_1.shadowMatrices[1] = lightData.directionalLights[light_2.dataIndex].shadowMatrices[1];
            dl_1.shadowMatrices[2] = lightData.directionalLights[light_2.dataIndex].shadowMatrices[2];
            dl_1.shadowInvMatrices[0] = lightData.directionalLights[light_2.dataIndex].shadowInvMatrices[0];
            dl_1.shadowInvMatrices[1] = lightData.directionalLights[light_2.dataIndex].shadowInvMatrices[1];
            dl_1.shadowInvMatrices[2] = lightData.directionalLights[light_2.dataIndex].shadowInvMatrices[2];
            vec3 L_1 = normalize(-dl_1.direction.xyz);
            vec3 H = normalize(V + L_1);
            float shadowFactor_1 = 1.0;
            if (light_2.shadowIndex != 4294967295u)
            {
                int shadowMapBase_1 = int(light_2.shadowIndex) * 3;
                DirectionalLight param_18 = dl_1;
                int param_19 = shadowMapBase_1;
                vec3 param_20 = N;
                vec3 param_21 = L_1;
                vec3 param_22 = fragWorldPos;
                vec3 param_23 = camera.cameraPosition.xyz;
                vec3 param_24 = camera.cameraForward.xyz;
                vec2 param_25 = gl_FragCoord.xy;
                shadowFactor_1 = ComputeDirectionalShadowFactor(param_18, param_19, param_20, param_21, param_22, param_23, param_24, param_25);
            }
            vec3 lightColor = light_2.colorIntensity.xyz * light_2.colorIntensity.w;
            float specularStrength = max(light_2.specular.x, 0.0);
            float shininess = max(light_2.specular.y, 1.0);
            float NdotL = max(dot(N, L_1), 0.0);
            vec3 F0_1 = mix(vec3(0.039999999105930328369140625), albedo, vec3(metalness));
            float fresnelAmount_1 = clamp(light_2.specular.z, 0.0, 1.0);
            float param_26 = max(dot(H, V), 0.0);
            vec3 param_27 = F0_1;
            vec3 Fschlick = FresnelSchlick(param_26, param_27);
            vec3 F = mix(F0_1, Fschlick, vec3(fresnelAmount_1));
            float smoothness_1 = 1.0 - roughness;
            vec4 param_28 = light_2.specular;
            uint highlightType = GetSpecularHighlightType(param_28);
            vec3 specularBrdf = vec3(0.0);
            if (highlightType == 1u)
            {
                vec3 param_29 = N;
                vec3 param_30 = V;
                vec3 param_31 = L_1;
                vec3 param_32 = F;
                float param_33 = shininess;
                specularBrdf = SpecularPhong(param_29, param_30, param_31, param_32, param_33);
            }
            else
            {
                if (highlightType == 2u)
                {
                    vec3 param_34 = N;
                    vec3 param_35 = H;
                    vec3 param_36 = F;
                    float param_37 = shininess;
                    specularBrdf = SpecularBlinnPhong(param_34, param_35, param_36, param_37);
                }
                else
                {
                    float lightShininessToRoughness = clamp(sqrt(2.0 / (shininess + 2.0)), 0.0500000007450580596923828125, 1.0);
                    float effectiveRoughness = max(roughness * lightShininessToRoughness, 0.04500000178813934326171875);
                    vec3 param_38 = N;
                    vec3 param_39 = V;
                    vec3 param_40 = L_1;
                    vec3 param_41 = H;
                    vec3 param_42 = F;
                    float param_43 = effectiveRoughness;
                    specularBrdf = SpecularCookTorrance(param_38, param_39, param_40, param_41, param_42, param_43);
                }
            }
            vec3 specular = (specularBrdf * specularStrength) * (smoothness_1 * smoothness_1);
            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);
            vec3 diffuse = (kD * albedo) / vec3(3.1415927410125732421875);
            color_1 += ((((diffuse + specular) * lightColor) * NdotL) * shadowFactor_1);
        }
    }
    vec3 F0_2 = mix(vec3(0.039999999105930328369140625), albedo, vec3(metalness));
    float smoothness_2 = 1.0 - roughness;
    float reflectionWeight_1 = clamp(metalness, 0.0, 1.0);
    if (reflectionWeight_1 > 9.9999997473787516355514526367188e-05)
    {
        vec3 R_1 = reflect(-V, N);
        vec3 env_1 = texture(skyboxTex, R_1).xyz * camera.skyTint.xyz;
        if (dot(env_1, env_1) < 9.9999999747524270787835121154785e-07)
        {
            env_1 = camera.skyTint.xyz;
        }
        float param_44 = max(dot(N, V), 0.0);
        vec3 param_45 = F0_2;
        vec3 FenvSchlick_1 = FresnelSchlick(param_44, param_45);
        vec3 Fenv_1 = mix(F0_2, FenvSchlick_1, vec3(fresnelAmountEnv));
        color_1 += (((env_1 * Fenv_1) * reflectionWeight_1) * (smoothness_2 * smoothness_2));
    }
    color_1 += emissiveScene;
    outSceneColor = vec4(color_1 + ((neonSample * 0.100000001490116119384765625) * glowMask), alpha);
    outGlowColor = vec4(glowColor, glowMask);
    outDepthColor = vec4(hbaoDepth, 0.0, 0.0, 1.0);
}

