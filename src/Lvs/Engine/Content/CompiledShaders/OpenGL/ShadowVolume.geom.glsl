#version 450
layout(triangles_adjacency) in;
layout(max_vertices = 18, triangle_strip) out;

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

struct ShadowVolumePush
{
    uvec4 data;
    vec4 lightDirExtrude;
    vec4 params;
};

uniform ShadowVolumePush pushData;

layout(location = 0) in vec3 vWorldPos[6];

float FacingToLight(vec3 a, vec3 b, vec3 c, vec3 lightRayDir)
{
    vec3 n = cross(b - a, c - a);
    return sign(dot(n, -lightRayDir));
}

vec4 ToClip(vec3 worldPos)
{
    return (camera.projection * camera.view) * vec4(worldPos, 1.0);
}

void EmitTriClip(vec4 a, vec4 b, vec4 c)
{
    gl_Position = a;
    EmitVertex();
    gl_Position = b;
    EmitVertex();
    gl_Position = c;
    EmitVertex();
    EndPrimitive();
}

void EmitQuadClip(vec4 a, vec4 b, vec4 b2, vec4 a2)
{
    gl_Position = a;
    EmitVertex();
    gl_Position = b;
    EmitVertex();
    gl_Position = a2;
    EmitVertex();
    gl_Position = b2;
    EmitVertex();
    EndPrimitive();
}

void main()
{
    vec3 v0 = vWorldPos[0];
    vec3 a0 = vWorldPos[1];
    vec3 v1 = vWorldPos[2];
    vec3 a1 = vWorldPos[3];
    vec3 v2 = vWorldPos[4];
    vec3 a2 = vWorldPos[5];
    vec3 lightRayDir = normalize(pushData.lightDirExtrude.xyz);
    float extrudeDist = max(pushData.lightDirExtrude.w, 1.0);
    float bias = pushData.params.x;
    vec3 param = v0;
    vec3 param_1 = v1;
    vec3 param_2 = v2;
    vec3 param_3 = lightRayDir;
    float f = FacingToLight(param, param_1, param_2, param_3);
    bool frontFacing = f > 0.0;
    vec3 bv0 = v0;
    vec3 bv1 = v1;
    vec3 bv2 = v2;
    if (abs(bias) > 0.0)
    {
        vec3 off = (-lightRayDir) * bias;
        bv0 += off;
        bv1 += off;
        bv2 += off;
    }
    vec3 e0 = bv0 + (lightRayDir * extrudeDist);
    vec3 e1 = bv1 + (lightRayDir * extrudeDist);
    vec3 e2 = bv2 + (lightRayDir * extrudeDist);
    vec3 param_4 = bv0;
    vec4 c0 = ToClip(param_4);
    vec3 param_5 = bv1;
    vec4 c1 = ToClip(param_5);
    vec3 param_6 = bv2;
    vec4 c2 = ToClip(param_6);
    vec4 infClip = (camera.projection * camera.view) * vec4(lightRayDir, 0.0);
    bool useInfiniteExtrusion = infClip.w > 9.9999997473787516355514526367188e-06;
    vec4 _231;
    if (useInfiniteExtrusion)
    {
        _231 = infClip;
    }
    else
    {
        vec3 param_7 = e0;
        _231 = ToClip(param_7);
    }
    vec4 ce0 = _231;
    vec4 _242;
    if (useInfiniteExtrusion)
    {
        _242 = infClip;
    }
    else
    {
        vec3 param_8 = e1;
        _242 = ToClip(param_8);
    }
    vec4 ce1 = _242;
    vec4 _253;
    if (useInfiniteExtrusion)
    {
        _253 = infClip;
    }
    else
    {
        vec3 param_9 = e2;
        _253 = ToClip(param_9);
    }
    vec4 ce2 = _253;
    int capMode = int(pushData.params.y + 0.5);
    bool emitNearCap = false;
    bool emitFarCap = false;
    if (capMode == 0)
    {
        emitNearCap = frontFacing;
        emitFarCap = !frontFacing;
    }
    else
    {
        if (capMode == 1)
        {
            emitNearCap = !frontFacing;
            emitFarCap = frontFacing;
        }
        else
        {
            emitNearCap = false;
            emitFarCap = false;
        }
    }
    if (useInfiniteExtrusion)
    {
        emitFarCap = false;
    }
    if (emitNearCap)
    {
        vec4 param_10 = c0;
        vec4 param_11 = c1;
        vec4 param_12 = c2;
        EmitTriClip(param_10, param_11, param_12);
    }
    if (emitFarCap)
    {
        vec4 param_13 = ce2;
        vec4 param_14 = ce1;
        vec4 param_15 = ce0;
        EmitTriClip(param_13, param_14, param_15);
    }
    if (frontFacing)
    {
        bool _319 = distance(a0, v0) < 1.0000000116860974230803549289703e-07;
        bool _327;
        if (!_319)
        {
            _327 = distance(a0, v1) < 1.0000000116860974230803549289703e-07;
        }
        else
        {
            _327 = _319;
        }
        bool missing0 = _327;
        bool _332 = distance(a1, v1) < 1.0000000116860974230803549289703e-07;
        bool _340;
        if (!_332)
        {
            _340 = distance(a1, v2) < 1.0000000116860974230803549289703e-07;
        }
        else
        {
            _340 = _332;
        }
        bool missing1 = _340;
        bool _345 = distance(a2, v2) < 1.0000000116860974230803549289703e-07;
        bool _353;
        if (!_345)
        {
            _353 = distance(a2, v0) < 1.0000000116860974230803549289703e-07;
        }
        else
        {
            _353 = _345;
        }
        bool missing2 = _353;
        float _356;
        if (missing0)
        {
            _356 = -1.0;
        }
        else
        {
            vec3 param_16 = v1;
            vec3 param_17 = v0;
            vec3 param_18 = a0;
            vec3 param_19 = lightRayDir;
            _356 = FacingToLight(param_16, param_17, param_18, param_19);
        }
        float af0 = _356;
        float _373;
        if (missing1)
        {
            _373 = -1.0;
        }
        else
        {
            vec3 param_20 = v2;
            vec3 param_21 = v1;
            vec3 param_22 = a1;
            vec3 param_23 = lightRayDir;
            _373 = FacingToLight(param_20, param_21, param_22, param_23);
        }
        float af1 = _373;
        float _389;
        if (missing2)
        {
            _389 = -1.0;
        }
        else
        {
            vec3 param_24 = v0;
            vec3 param_25 = v2;
            vec3 param_26 = a2;
            vec3 param_27 = lightRayDir;
            _389 = FacingToLight(param_24, param_25, param_26, param_27);
        }
        float af2 = _389;
        if (af0 <= 0.0)
        {
            vec4 param_28 = c0;
            vec4 param_29 = c1;
            vec4 param_30 = ce1;
            vec4 param_31 = ce0;
            EmitQuadClip(param_28, param_29, param_30, param_31);
        }
        if (af1 <= 0.0)
        {
            vec4 param_32 = c1;
            vec4 param_33 = c2;
            vec4 param_34 = ce2;
            vec4 param_35 = ce1;
            EmitQuadClip(param_32, param_33, param_34, param_35);
        }
        if (af2 <= 0.0)
        {
            vec4 param_36 = c2;
            vec4 param_37 = c0;
            vec4 param_38 = ce0;
            vec4 param_39 = ce2;
            EmitQuadClip(param_36, param_37, param_38, param_39);
        }
    }
}

