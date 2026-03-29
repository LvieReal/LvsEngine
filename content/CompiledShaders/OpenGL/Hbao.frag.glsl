#version 450

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

struct HbaoSettings
{
    vec4 params0;
    vec4 params1;
    vec4 params2;
};

uniform HbaoSettings pushData;

layout(binding = 1) uniform sampler2D depthTex;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

float ViewSpaceZFromReversedInfiniteDepth(float depth)
{
    float nearPlane = camera.projection[3].z;
    if (camera.projection[2].z > 0.5)
    {
        nearPlane *= 0.5;
    }
    return (-nearPlane) / max(depth, 9.9999999747524270787835121154785e-07);
}

vec3 UVToViewSpace(vec2 uv, float z)
{
    vec2 ndc = (uv * 2.0) - vec2(1.0);
    vec2 focalLen = abs(vec2(camera.projection[0].x, camera.projection[1].y));
    vec2 viewXY = (ndc * (-z)) / max(focalLen, vec2(9.9999999747524270787835121154785e-07));
    return vec3(viewXY, z);
}

vec3 GetViewPos(vec2 uv)
{
    float param = texture(depthTex, uv).x;
    float z = ViewSpaceZFromReversedInfiniteDepth(param);
    vec2 param_1 = uv;
    float param_2 = z;
    return UVToViewSpace(param_1, param_2);
}

float Length2(vec3 v)
{
    return dot(v, v);
}

vec3 MinDiff(vec3 p, vec3 pr, vec3 pl)
{
    vec3 v1 = pr - p;
    vec3 v2 = p - pl;
    vec3 param = v1;
    vec3 param_1 = v2;
    return mix(v2, v1, bvec3(Length2(param) < Length2(param_1)));
}

float Hash12(vec2 p)
{
    vec3 p3 = fract(p.xyx * 0.103100001811981201171875);
    p3 += vec3(dot(p3, p3.yzx + vec3(33.3300018310546875)));
    return fract((p3.x + p3.y) * p3.z);
}

void ComputeSteps(out vec2 stepSizeUv, inout float numSteps, float rayRadiusPix, float rand, float maxRadiusPixels)
{
    numSteps = min(numSteps, rayRadiusPix);
    float stepSizePix = rayRadiusPix / (numSteps + 1.0);
    float maxNumSteps = maxRadiusPixels / max(stepSizePix, 9.9999997473787516355514526367188e-05);
    if (maxNumSteps < numSteps)
    {
        numSteps = floor(maxNumSteps + rand);
        numSteps = max(numSteps, 1.0);
        stepSizePix = maxRadiusPixels / numSteps;
    }
    stepSizeUv = vec2(pushData.params2.z, pushData.params2.w) * stepSizePix;
}

vec2 RotateDirections(vec2 dir, vec2 cosSin)
{
    return vec2((dir.x * cosSin.x) - (dir.y * cosSin.y), (dir.x * cosSin.y) + (dir.y * cosSin.x));
}

vec2 SnapUVOffset(vec2 uv, vec2 aoRes, vec2 invAoRes)
{
    return round(uv * aoRes) * invAoRes;
}

float InvLength(vec2 v)
{
    return inversesqrt(max(dot(v, v), 1.0000000133514319600180897396058e-10));
}

float BiasedTangent(vec3 v, float tanBias)
{
    vec2 param = v.xy;
    return (v.z * InvLength(param)) + tanBias;
}

float TanToSin(float t)
{
    return t * inversesqrt((t * t) + 1.0);
}

float Tangent(vec3 p, vec3 s)
{
    vec2 param = s.xy - p.xy;
    return (-(p.z - s.z)) * InvLength(param);
}

float Falloff(float d2, float negInvR2)
{
    return (d2 * negInvR2) + 1.0;
}

float HorizonOcclusion(inout vec2 deltaUV, vec3 p, vec3 dPdu, vec3 dPdv, float randStep, float numSamples, float tanBias, float r2, float negInvR2, vec2 aoRes, vec2 invAoRes)
{
    float ao = 0.0;
    vec2 param = deltaUV * randStep;
    vec2 param_1 = aoRes;
    vec2 param_2 = invAoRes;
    vec2 uv = fragUv + SnapUVOffset(param, param_1, param_2);
    vec2 param_3 = deltaUV;
    vec2 param_4 = aoRes;
    vec2 param_5 = invAoRes;
    deltaUV = SnapUVOffset(param_3, param_4, param_5);
    vec3 t = (dPdu * deltaUV.x) + (dPdv * deltaUV.y);
    vec3 param_6 = t;
    float param_7 = tanBias;
    float tanH = BiasedTangent(param_6, param_7);
    float param_8 = tanH;
    float sinH = TanToSin(param_8);
    for (float s = 1.0; s <= numSamples; s += 1.0)
    {
        uv += deltaUV;
        vec2 param_9 = clamp(uv, vec2(0.0), vec2(1.0));
        vec3 sPos = GetViewPos(param_9);
        vec3 param_10 = p;
        vec3 param_11 = sPos;
        float tanS = Tangent(param_10, param_11);
        vec3 param_12 = sPos - p;
        float d2 = Length2(param_12);
        if ((d2 < r2) && (tanS > tanH))
        {
            float param_13 = tanS;
            float sinS = TanToSin(param_13);
            float param_14 = d2;
            float param_15 = negInvR2;
            ao += (Falloff(param_14, param_15) * (sinS - sinH));
            tanH = tanS;
            sinH = sinS;
        }
    }
    return ao;
}

void main()
{
    vec2 aoRes = vec2(pushData.params2.x, pushData.params2.y);
    vec2 invAoRes = vec2(pushData.params2.z, pushData.params2.w);
    vec2 param = fragUv;
    vec3 p = GetViewPos(param);
    vec2 param_1 = fragUv + vec2(invAoRes.x, 0.0);
    vec3 pr = GetViewPos(param_1);
    vec2 param_2 = fragUv + vec2(-invAoRes.x, 0.0);
    vec3 pl = GetViewPos(param_2);
    vec2 param_3 = fragUv + vec2(0.0, invAoRes.y);
    vec3 pt = GetViewPos(param_3);
    vec2 param_4 = fragUv + vec2(0.0, -invAoRes.y);
    vec3 pb = GetViewPos(param_4);
    vec3 param_5 = p;
    vec3 param_6 = pr;
    vec3 param_7 = pl;
    vec3 dPdu = MinDiff(param_5, param_6, param_7);
    vec3 param_8 = p;
    vec3 param_9 = pt;
    vec3 param_10 = pb;
    vec3 dPdv = MinDiff(param_8, param_9, param_10) * (aoRes.y * invAoRes.x);
    vec2 param_11 = floor(gl_FragCoord.xy);
    float rand = Hash12(param_11);
    float theta = (rand * 2.0) * 3.1415927410125732421875;
    vec2 cosSin = vec2(cos(theta), sin(theta));
    float randStep = fract(rand * 13.36999988555908203125);
    float radius = max(0.0, pushData.params0.x);
    float r2 = radius * radius;
    float _565;
    if (r2 > 0.0)
    {
        _565 = (-1.0) / r2;
    }
    else
    {
        _565 = 0.0;
    }
    float negInvR2 = _565;
    float tanBias = pushData.params0.z;
    float maxRadiusPixels = max(1.0, pushData.params0.w);
    vec2 focalLen = abs(vec2(camera.projection[0].x, camera.projection[1].y));
    vec2 rayRadiusUV = (focalLen * (0.5 * radius)) / vec2(max(9.9999997473787516355514526367188e-05, -p.z));
    float rayRadiusPix = rayRadiusUV.x * aoRes.x;
    float ao = 1.0;
    float directions = max(1.0, pushData.params1.y);
    float samples = max(1.0, pushData.params1.z);
    if ((rayRadiusPix > 1.0) && (r2 > 0.0))
    {
        ao = 0.0;
        float numSteps = samples;
        vec2 stepSizeUV = vec2(0.0);
        vec2 param_12 = stepSizeUV;
        float param_13 = numSteps;
        float param_14 = rayRadiusPix;
        float param_15 = randStep;
        float param_16 = maxRadiusPixels;
        ComputeSteps(param_12, param_13, param_14, param_15, param_16);
        stepSizeUV = param_12;
        numSteps = param_13;
        float alpha = 6.283185482025146484375 / directions;
        for (float d = 0.0; d < directions; d += 1.0)
        {
            float a = alpha * d;
            vec2 param_17 = vec2(cos(a), sin(a));
            vec2 param_18 = cosSin;
            vec2 dir = RotateDirections(param_17, param_18);
            vec2 deltaUV = dir * stepSizeUV;
            vec2 param_19 = deltaUV;
            vec3 param_20 = p;
            vec3 param_21 = dPdu;
            vec3 param_22 = dPdv;
            float param_23 = randStep;
            float param_24 = numSteps;
            float param_25 = tanBias;
            float param_26 = r2;
            float param_27 = negInvR2;
            vec2 param_28 = aoRes;
            vec2 param_29 = invAoRes;
            float _689 = HorizonOcclusion(param_19, param_20, param_21, param_22, param_23, param_24, param_25, param_26, param_27, param_28, param_29);
            ao += _689;
        }
        ao = 1.0 - ((ao / directions) * max(0.0, pushData.params0.y));
    }
    float power = max(0.00999999977648258209228515625, pushData.params1.x);
    float aoFactor = pow(clamp(ao, 0.0, 1.0), power);
    outColor = vec4(aoFactor, aoFactor, aoFactor, 1.0);
}

