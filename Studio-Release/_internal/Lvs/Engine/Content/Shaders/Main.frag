#version 460 core

in vec3 FragColor;
in vec3 FragDirectionalColor;
in vec3 FragWorldPos;
in vec3 FragNormal;
in vec3 FragLocalPos;
in vec3 FragLocalNormal;

out vec4 Color;

uniform vec3 CameraPosition;
uniform vec3 CameraForward;

uniform vec3 MeshSize;
uniform float MeshAlpha;
uniform float MeshReflective;

uniform int IgnoreDiffuseSpecular;
uniform int EnableShadow;

uniform samplerCube Sky;
uniform sampler2DShadow DirectionalShadowMap;
uniform sampler2DShadow DirectionalShadowMap0;
uniform sampler2DShadow DirectionalShadowMap1;
uniform sampler2DShadow DirectionalShadowMap2;
uniform sampler3D DirectionalShadowJitter;

uniform mat4 DirectionalLightShadowMatrix;
uniform mat4 DirectionalLightShadowMatrices[3];
uniform vec3 LightDirection;
uniform float DirectionalShadowBias;
uniform float DirectionalShadowSoftness;
uniform float DirectionalShadowFadeWidth;
uniform int DirectionalShadowTapCount;
uniform int DirectionalShadowCascadeCount;
uniform float DirectionalShadowCascadeSplit0;
uniform float DirectionalShadowCascadeSplit1;
uniform vec2 DirectionalShadowJitterScale;

uniform int TopSurfaceType;
uniform int BottomSurfaceType;
uniform int FrontSurfaceType;
uniform int BackSurfaceType;
uniform int LeftSurfaceType;
uniform int RightSurfaceType;
uniform int SurfaceEnabled;
uniform vec2 SurfaceScale;

uniform vec2 SurfaceAtlasGrid;

uniform sampler2D SurfaceAtlas;

#define SMOOTH 0
#define SAMPLES_COUNT 64
#define SAMPLES_COUNT_DIV_2 32
#define INV_SAMPLES_COUNT (1.0 / float(SAMPLES_COUNT))

int getSurfaceType(vec3 normal)
{
    vec3 n = normalize(normal);
    vec3 a = abs(n);

    if (a.y >= a.x && a.y >= a.z)
        return (n.y > 0.0) ? TopSurfaceType : BottomSurfaceType;
    if (a.x >= a.y && a.x >= a.z)
        return (n.x > 0.0) ? RightSurfaceType : LeftSurfaceType;
    return (n.z > 0.0) ? FrontSurfaceType : BackSurfaceType;
}

vec2 getFaceUV(vec3 pos, vec3 normal)
{
    vec3 scaledPos = pos * MeshSize;
    vec3 n = normalize(normal);
    vec3 a = abs(n);
    vec2 uv;

    if (a.x >= a.y && a.x >= a.z)
        uv = scaledPos.zy;
    else if (a.y >= a.x && a.y >= a.z)
        uv = scaledPos.xz;
    else
        uv = scaledPos.xy;

    return fract(uv * SurfaceScale);
}

vec3 sampleSurface(int type, vec2 uv)
{
    if (type == SMOOTH || SurfaceEnabled == 0)
        return vec3(1.0);

    float cols = max(1.0, SurfaceAtlasGrid.x);
    float rows = max(1.0, SurfaceAtlasGrid.y);
    float tileWidth = 1.0 / cols;
    float tileHeight = 1.0 / rows;

    float tileIndex = clamp(float(type - 1), 0.0, (cols * rows) - 1.0);
    float tileX = mod(tileIndex, cols);
    float tileY = floor(tileIndex / cols);

    vec2 base = vec2(tileX * tileWidth, tileY * tileHeight);
    vec2 tileUV = uv * vec2(tileWidth, tileHeight);

    return texture(SurfaceAtlas, base + tileUV).rgb;
}

vec2 getDirectionalShadowTexelSize(int cascadeIndex)
{
    if (cascadeIndex == 1)
        return 1.0 / vec2(textureSize(DirectionalShadowMap1, 0));
    if (cascadeIndex == 2)
        return 1.0 / vec2(textureSize(DirectionalShadowMap2, 0));
    return 1.0 / vec2(textureSize(DirectionalShadowMap0, 0));
}

float sampleDirectionalShadowMapProj(int cascadeIndex, vec4 shadowCoord)
{
    if (cascadeIndex == 1)
        return textureProj(DirectionalShadowMap1, shadowCoord);
    if (cascadeIndex == 2)
        return textureProj(DirectionalShadowMap2, shadowCoord);
    return textureProj(DirectionalShadowMap0, shadowCoord);
}

mat4 getDirectionalCascadeMatrix(int cascadeIndex)
{
    if (cascadeIndex == 1)
        return DirectionalLightShadowMatrices[1];
    if (cascadeIndex == 2)
        return DirectionalLightShadowMatrices[2];
    return DirectionalLightShadowMatrices[0];
}

float sampleDirectionalAdaptivePCF64(
    int cascadeIndex,
    vec4 baseShadowCoord,
    float radiusTexels,
    float ndotl
)
{
    float radius = max(0.0, radiusTexels);
    if (radius <= 1e-6)
        return sampleDirectionalShadowMapProj(cascadeIndex, baseShadowCoord);

    float texelScale = max(getDirectionalShadowTexelSize(cascadeIndex).x, getDirectionalShadowTexelSize(cascadeIndex).y);
    float fsize = radius * texelScale;
    vec3 jcoord = vec3(gl_FragCoord.xy * DirectionalShadowJitterScale, 0.0);
    vec4 smCoord = baseShadowCoord;
    float shadow = 0.0;

    // 8 cheap test taps (4 jitter fetches with 2 offsets each).
    for (int i = 0; i < 4; i++)
    {
        vec4 offset = (texture(DirectionalShadowJitter, jcoord) * 2.0) - 1.0;
        jcoord.z += 1.0 / float(SAMPLES_COUNT_DIV_2);

        smCoord.xy = (offset.xy * fsize) + baseShadowCoord.xy;
        shadow += sampleDirectionalShadowMapProj(cascadeIndex, smCoord) * (1.0 / 8.0);

        smCoord.xy = (offset.zw * fsize) + baseShadowCoord.xy;
        shadow += sampleDirectionalShadowMapProj(cascadeIndex, smCoord) * (1.0 / 8.0);
    }

    // GPU Gems 2 branch condition: refine only in penumbra and only on lit-facing side.
    if (ndotl > 0.0 && shadow > 0.0 && shadow < 1.0)
    {
        shadow *= (1.0 / 8.0);
        for (int i = 0; i < (SAMPLES_COUNT_DIV_2 - 4); i++)
        {
            vec4 offset = (texture(DirectionalShadowJitter, jcoord) * 2.0) - 1.0;
            jcoord.z += 1.0 / float(SAMPLES_COUNT_DIV_2);

            smCoord.xy = (offset.xy * fsize) + baseShadowCoord.xy;
            shadow += sampleDirectionalShadowMapProj(cascadeIndex, smCoord) * INV_SAMPLES_COUNT;

            smCoord.xy = (offset.zw * fsize) + baseShadowCoord.xy;
            shadow += sampleDirectionalShadowMapProj(cascadeIndex, smCoord) * INV_SAMPLES_COUNT;
        }
    }

    return shadow;
}

float computeDirectionalBias(int cascadeIndex, float pcfRadiusTexels)
{
    vec2 texelSize = getDirectionalShadowTexelSize(cascadeIndex);

    float texelScale = max(texelSize.x, texelSize.y);

    float NdotL = clamp(dot(normalize(FragNormal), -normalize(LightDirection)), 0.0, 1.0);

    float slope = sqrt(1.0 - NdotL * NdotL) / max(NdotL, 0.05);

    const float slopeBias = 0.25;

    float biasTexels = DirectionalShadowBias + slopeBias * slope;

    biasTexels *= (1.0 + pcfRadiusTexels * 0.25);

    return biasTexels * texelScale;
}

float computeDirectionalShadow()
{
    if (EnableShadow == 0 || IgnoreDiffuseSpecular != 0)
        return 1.0;

    int cascadeCount = clamp(DirectionalShadowCascadeCount, 1, 3);
    float viewDepth = max(dot(FragWorldPos - CameraPosition, normalize(CameraForward)), 0.0);
    int cascadeIndex = 0;
    if (cascadeCount >= 3 && viewDepth > DirectionalShadowCascadeSplit1)
        cascadeIndex = 2;
    else if (cascadeCount >= 2 && viewDepth > DirectionalShadowCascadeSplit0)
        cascadeIndex = 1;

    vec4 lightClip = getDirectionalCascadeMatrix(cascadeIndex) * vec4(FragWorldPos, 1.0);
    if (abs(lightClip.w) <= 1e-6)
        return 1.0;

    vec3 lightNdc = lightClip.xyz / lightClip.w;
    vec2 shadowUv = (lightNdc.xy * 0.5) + 0.5;
    float receiverDepth = lightNdc.z;

    if (receiverDepth <= 0.0 || receiverDepth >= 1.0)
        return 1.0;

    float softness = clamp(DirectionalShadowSoftness, 0.0, 12.0);

    float bias = computeDirectionalBias(cascadeIndex, softness);
    float ndotl = clamp(dot(normalize(FragNormal), -normalize(LightDirection)), 0.0, 1.0);
    vec4 shadowCoord = vec4(shadowUv, clamp(receiverDepth - bias, 0.0, 1.0), 1.0);

    return sampleDirectionalAdaptivePCF64(cascadeIndex, shadowCoord, softness, ndotl);
}

void main()
{
    float directionalShadow = computeDirectionalShadow();
    vec3 nonDirectional = max(FragColor - FragDirectionalColor, vec3(0.0));
    vec3 albedo = nonDirectional + (FragDirectionalColor * directionalShadow);

    // float camToFrag = length(FragWorldPos - CameraPosition);

    if (MeshReflective != 0.0)
    {
        vec3 I = normalize(FragWorldPos - CameraPosition);
        vec3 R = reflect(I, normalize(FragNormal));
        albedo = mix(albedo, texture(Sky, R).rgb * albedo, MeshReflective);
    }

    int surfType = getSurfaceType(FragLocalNormal);
    vec2 faceUV = getFaceUV(FragLocalPos, FragLocalNormal);
    vec3 surfaceDetail = sampleSurface(surfType, faceUV);
    albedo *= surfaceDetail;

    Color = vec4(albedo, MeshAlpha);
}
