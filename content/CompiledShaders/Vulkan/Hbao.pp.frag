#version 450

layout(location = 0) in vec2 fragUv;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 ambient;
    vec4 skyTint;
    vec4 renderSettings;
    vec4 lightingSettings;
    vec4 cameraForward;
} camera;

layout(set = 0, binding = 1) uniform sampler2D depthTex;

layout(push_constant) uniform HbaoSettings {
    vec4 params0; // x: radius, y: strength, z: tanBias, w: maxRadiusPixels
    vec4 params1; // x: power, y: directions, z: samples, w: reserved
    vec4 params2; // x: aoResX, y: aoResY, z: invResX, w: invResY
} pushData;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265;

float Hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 RotateDirections(vec2 dir, vec2 cosSin) {
    return vec2(dir.x * cosSin.x - dir.y * cosSin.y, dir.x * cosSin.y + dir.y * cosSin.x);
}

float InvLength(vec2 v) {
    return inversesqrt(max(dot(v, v), 1e-10));
}

float TanToSin(float t) {
    return t * inversesqrt(t * t + 1.0);
}

float Tangent(vec3 v) {
    return v.z * InvLength(v.xy);
}

float BiasedTangent(vec3 v, float tanBias) {
    return v.z * InvLength(v.xy) + tanBias;
}

float Tangent(vec3 p, vec3 s) {
    return -(p.z - s.z) * InvLength(s.xy - p.xy);
}

float Length2(vec3 v) {
    return dot(v, v);
}

vec3 MinDiff(vec3 p, vec3 pr, vec3 pl) {
    vec3 v1 = pr - p;
    vec3 v2 = p - pl;
    return (Length2(v1) < Length2(v2)) ? v1 : v2;
}

vec2 SnapUVOffset(vec2 uv, vec2 aoRes, vec2 invAoRes) {
    return round(uv * aoRes) * invAoRes;
}

float ViewSpaceZFromReversedInfiniteDepth(float depth) {
    float nearPlane = camera.projection[3][2];
    // When using an OpenGL clip-space projection remap (ApplyOpenGLClipDepthRemap),
    // the projection's reversed-infinite near plane term becomes 2*near.
    // Detect the remapped matrix via projection[2][2] ~= 1.
    if (camera.projection[2][2] > 0.5) {
        nearPlane *= 0.5;
    }
    return -nearPlane / max(depth, 1e-6);
}

vec3 UVToViewSpace(vec2 uv, float z) {
    vec2 ndc = uv * 2.0 - 1.0;
    vec2 focalLen = abs(vec2(camera.projection[0][0], camera.projection[1][1]));
    vec2 viewXY = ndc * (-z) / max(focalLen, vec2(1e-6));
    return vec3(viewXY, z);
}

vec3 GetViewPos(vec2 uv) {
    float z = ViewSpaceZFromReversedInfiniteDepth(texture(depthTex, uv).r);
    return UVToViewSpace(uv, z);
}

float Falloff(float d2, float negInvR2) {
    return d2 * negInvR2 + 1.0;
}

void ComputeSteps(inout vec2 stepSizeUv, inout float numSteps, float rayRadiusPix, float rand, float maxRadiusPixels) {
    numSteps = min(numSteps, rayRadiusPix);
    float stepSizePix = rayRadiusPix / (numSteps + 1.0);
    float maxNumSteps = maxRadiusPixels / max(stepSizePix, 1e-4);
    if (maxNumSteps < numSteps) {
        numSteps = floor(maxNumSteps + rand);
        numSteps = max(numSteps, 1.0);
        stepSizePix = maxRadiusPixels / numSteps;
    }
    stepSizeUv = stepSizePix * vec2(pushData.params2.z, pushData.params2.w);
}

float HorizonOcclusion(
    vec2 deltaUV,
    vec3 p,
    vec3 dPdu,
    vec3 dPdv,
    float randStep,
    float numSamples,
    float tanBias,
    float r2,
    float negInvR2,
    vec2 aoRes,
    vec2 invAoRes
) {
    float ao = 0.0;

    vec2 uv = fragUv + SnapUVOffset(randStep * deltaUV, aoRes, invAoRes);
    deltaUV = SnapUVOffset(deltaUV, aoRes, invAoRes);

    vec3 t = deltaUV.x * dPdu + deltaUV.y * dPdv;
    float tanH = BiasedTangent(t, tanBias);
    float sinH = TanToSin(tanH);

    for (float s = 1.0; s <= numSamples; s += 1.0) {
        uv += deltaUV;
        vec3 sPos = GetViewPos(clamp(uv, vec2(0.0), vec2(1.0)));
        float tanS = Tangent(p, sPos);
        float d2 = Length2(sPos - p);

        if (d2 < r2 && tanS > tanH) {
            float sinS = TanToSin(tanS);
            ao += Falloff(d2, negInvR2) * (sinS - sinH);
            tanH = tanS;
            sinH = sinS;
        }
    }

    return ao;
}

void main() {
    vec2 aoRes = vec2(pushData.params2.x, pushData.params2.y);
    vec2 invAoRes = vec2(pushData.params2.z, pushData.params2.w);

    vec3 p = GetViewPos(fragUv);
    vec3 pr = GetViewPos(fragUv + vec2(invAoRes.x, 0.0));
    vec3 pl = GetViewPos(fragUv + vec2(-invAoRes.x, 0.0));
    vec3 pt = GetViewPos(fragUv + vec2(0.0, invAoRes.y));
    vec3 pb = GetViewPos(fragUv + vec2(0.0, -invAoRes.y));

    vec3 dPdu = MinDiff(p, pr, pl);
    vec3 dPdv = MinDiff(p, pt, pb) * (aoRes.y * invAoRes.x);

    float rand = Hash12(floor(gl_FragCoord.xy));
    float theta = rand * 2.0 * PI;
    vec2 cosSin = vec2(cos(theta), sin(theta));
    float randStep = fract(rand * 13.37);

    float radius = max(0.0, pushData.params0.x);
    float r2 = radius * radius;
    float negInvR2 = (r2 > 0.0) ? (-1.0 / r2) : 0.0;
    float tanBias = pushData.params0.z;
    float maxRadiusPixels = max(1.0, pushData.params0.w);

    vec2 focalLen = abs(vec2(camera.projection[0][0], camera.projection[1][1]));
    vec2 rayRadiusUV = 0.5 * radius * focalLen / max(1e-4, -p.z);
    float rayRadiusPix = rayRadiusUV.x * aoRes.x;

    float ao = 1.0;
    float directions = max(1.0, pushData.params1.y);
    float samples = max(1.0, pushData.params1.z);

    if (rayRadiusPix > 1.0 && r2 > 0.0) {
        ao = 0.0;
        float numSteps = samples;
        vec2 stepSizeUV = vec2(0.0);
        ComputeSteps(stepSizeUV, numSteps, rayRadiusPix, randStep, maxRadiusPixels);

        float alpha = 2.0 * PI / directions;
        for (float d = 0.0; d < directions; d += 1.0) {
            float a = alpha * d;
            vec2 dir = RotateDirections(vec2(cos(a), sin(a)), cosSin);
            vec2 deltaUV = dir * stepSizeUV;
            ao += HorizonOcclusion(
                deltaUV,
                p,
                dPdu,
                dPdv,
                randStep,
                numSteps,
                tanBias,
                r2,
                negInvR2,
                aoRes,
                invAoRes
            );
        }

        ao = 1.0 - ao / directions * max(0.0, pushData.params0.y);
    }

    float power = max(0.01, pushData.params1.x);
    float aoFactor = pow(clamp(ao, 0.0, 1.0), power);
    outColor = vec4(aoFactor, aoFactor, aoFactor, 1.0);
}

