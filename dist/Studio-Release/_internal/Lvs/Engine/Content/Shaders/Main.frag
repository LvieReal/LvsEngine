#version 460 core

in vec3 FragColor;
in vec3 FragWorldPos;
in vec3 FragNormal;
in vec3 FragLocalPos;
in vec3 FragLocalNormal;

out vec4 Color;

uniform vec3 CameraPosition;

uniform vec3 MeshSize;
uniform float MeshAlpha;
uniform float MeshReflective;

uniform samplerCube Sky;

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

void main()
{
    vec3 albedo = FragColor;

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