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

layout(binding = 2) uniform sampler2D surfaceAtlas;
layout(binding = 6) uniform sampler2D neonTexture;

layout(location = 3) in vec3 fragLocalNormal;
layout(location = 2) in vec3 fragLocalPos;
layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outGlowColor;
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragWorldPos;

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
    bool _282 = a.y >= a.x;
    bool _290;
    if (_282)
    {
        _290 = a.y >= a.z;
    }
    else
    {
        _290 = _282;
    }
    if (_290)
    {
        int _296;
        if (n.y > 0.0)
        {
            _296 = GetTopSurfaceType();
        }
        else
        {
            _296 = GetBottomSurfaceType();
        }
        return _296;
    }
    bool _308 = a.x >= a.y;
    bool _316;
    if (_308)
    {
        _316 = a.x >= a.z;
    }
    else
    {
        _316 = _308;
    }
    if (_316)
    {
        int _322;
        if (n.x > 0.0)
        {
            _322 = GetRightSurfaceType();
        }
        else
        {
            _322 = GetLeftSurfaceType();
        }
        return _322;
    }
    int _333;
    if (n.z > 0.0)
    {
        _333 = GetFrontSurfaceType();
    }
    else
    {
        _333 = GetBackSurfaceType();
    }
    return _333;
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
    bool _356 = a.x >= a.y;
    bool _364;
    if (_356)
    {
        _364 = a.x >= a.z;
    }
    else
    {
        _364 = _356;
    }
    vec2 uv;
    if (_364)
    {
        uv = scaledPos.zy;
    }
    else
    {
        bool _375 = a.y >= a.x;
        bool _383;
        if (_375)
        {
            _383 = a.y >= a.z;
        }
        else
        {
            _383 = _375;
        }
        if (_383)
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
    bool _474 = surfaceType == 0;
    bool _480;
    if (!_474)
    {
        _480 = !IsSurfaceEnabled();
    }
    else
    {
        _480 = _474;
    }
    if (_480)
    {
        return vec3(1.0);
    }
    int param = surfaceType;
    vec2 param_1 = uv;
    return texture(surfaceAtlas, GetSurfaceAtlasUV(param, param_1)).xyz;
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

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + ((vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0));
}

void main()
{
    vec3 albedo = pushData.baseColor.xyz;
    vec3 param = fragLocalNormal;
    int surfaceType = GetSurfaceType(param);
    vec3 param_1 = fragLocalPos;
    vec3 param_2 = fragLocalNormal;
    vec2 faceUV = GetFaceUV(param_1, param_2);
    int param_3 = surfaceType;
    vec2 param_4 = faceUV;
    vec3 surfaceDetail = SampleSurfaceColor(param_3, param_4);
    albedo *= surfaceDetail;
    float alpha = pushData.baseColor.w;
    float ignoreLighting = pushData.material.w;
    float metalness = clamp(pushData.material.x, 0.0, 1.0);
    float roughness = clamp(pushData.material.y, 0.0, 1.0);
    float emissive = max(pushData.material.z, 0.0);
    float neonEnabled = camera.renderSettings.z;
    float allowBlackNeon = camera.renderSettings.w;
    vec3 emissiveScene = (albedo * emissive) * 4.0;
    float albedoL2 = dot(albedo, albedo);
    float glowMask = float(((neonEnabled > 0.5) && (ignoreLighting <= 0.5)) && (emissive > 9.9999997473787516355514526367188e-05));
    vec3 glowBase = albedo;
    float blackNeonGlowBoost = 1.0;
    if (((allowBlackNeon > 0.5) && (albedoL2 < 9.9999999747524270787835121154785e-07)) && (glowMask > 0.0))
    {
        glowBase = vec3(0.0039215688593685626983642578125);
        blackNeonGlowBoost = 2.0;
    }
    vec3 glowColor = (((glowBase * emissive) * 8.0) * blackNeonGlowBoost) * glowMask;
    vec2 neonUv = gl_FragCoord.xy / vec2(max(textureSize(neonTexture, 0), ivec2(1)));
    vec3 _615;
    if (neonEnabled > 0.5)
    {
        _615 = texture(neonTexture, neonUv).xyz;
    }
    else
    {
        _615 = vec3(0.0);
    }
    vec3 neonSample = _615;
    if (ignoreLighting > 0.5)
    {
        outSceneColor = vec4((albedo + emissiveScene) + ((neonSample * 0.100000001490116119384765625) * glowMask), alpha);
        outGlowColor = vec4(0.0);
        return;
    }
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(camera.cameraPosition.xyz - fragWorldPos);
    vec3 color = camera.ambient.xyz * albedo;
    vec3 L = normalize(-camera.lightDirection.xyz);
    vec3 H = normalize(V + L);
    vec3 lightColor = camera.lightColorIntensity.xyz * camera.lightColorIntensity.w;
    float specularStrength = max(camera.lightSpecular.x, 0.0);
    float shininess = max(camera.lightSpecular.y, 1.0);
    float NdotL = max(dot(N, L), 0.0);
    vec3 F0 = mix(vec3(0.039999999105930328369140625), albedo, vec3(metalness));
    float lightShininessToRoughness = clamp(sqrt(2.0 / (shininess + 2.0)), 0.0500000007450580596923828125, 1.0);
    float effectiveRoughness = max(roughness * lightShininessToRoughness, 0.04500000178813934326171875);
    vec3 param_5 = N;
    vec3 param_6 = H;
    float param_7 = effectiveRoughness;
    float NDF = DistributionGGX(param_5, param_6, param_7);
    vec3 param_8 = N;
    vec3 param_9 = V;
    vec3 param_10 = L;
    float param_11 = effectiveRoughness;
    float G = GeometrySmith(param_8, param_9, param_10, param_11);
    float param_12 = max(dot(H, V), 0.0);
    vec3 param_13 = F0;
    vec3 F = FresnelSchlick(param_12, param_13);
    vec3 numerator = F * (NDF * G);
    float denominator = max((4.0 * max(dot(N, V), 0.0)) * NdotL, 9.9999997473787516355514526367188e-06);
    float smoothness = 1.0 - roughness;
    vec3 specular = ((numerator / vec3(denominator)) * specularStrength) * (smoothness * smoothness);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metalness);
    vec3 diffuse = (kD * albedo) / vec3(3.1415927410125732421875);
    color += (((diffuse + specular) * lightColor) * NdotL);
    color += emissiveScene;
    outSceneColor = vec4(color + ((neonSample * 0.100000001490116119384765625) * glowMask), alpha);
    outGlowColor = vec4(glowColor, glowMask);
}

