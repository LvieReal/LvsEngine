#version 450

layout(location = 0) in vec2 fragUv;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 ambient; // rgb: ambient color, a: ambient strength
    vec4 skyTint;
    vec4 renderSettings;
    vec4 lightingSettings;
    vec4 cameraForward;
} camera;

layout(set = 0, binding = 1) uniform sampler2D sceneColor;
layout(set = 0, binding = 2) uniform sampler2D glowColor;
layout(set = 0, binding = 16) uniform sampler2D aoTexture;
layout(set = 0, binding = 17) uniform sampler2D shadowVolumeMask;
layout(set = 0, binding = 18) uniform sampler2D depthColor;

layout(push_constant) uniform PostSettings {
    vec4 settings; // x: tonemapper, y: ditheringEnabled, z: neonEnabled, w: frameSeed
    vec4 aoTint;   // rgb: tint applied when AO factor is 0, a: neonAttenuation
} pushData;

layout(location = 0) out vec4 outColor;


// --- include begin: D:\VSCode\PROJECTS\LvsEngine\content\Shaders\Utils\Tonemapping.glsl
#ifndef LVS_TONEMAPPING_GLSL
#define LVS_TONEMAPPING_GLSL

const int TONEMAPPER_NONE = 1;
const int TONEMAPPER_COMPRESSION = 2;
const int TONEMAPPER_ACES = 3;
const int TONEMAPPER_AGX = 4;

vec3 LinearToDisplayGamma(vec3 x) {
    x = clamp(x, 0.0, 1.0);
    return pow(x, vec3(1.0 / 2.2));
}

vec3 TonemapCompression(vec3 x) {
    return x / (x + vec3(1.0));
}

vec3 TonemapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 TonemapAgXApprox(vec3 x) {
    x = max(x, vec3(0.0));
    vec3 logv = log2(max(x, vec3(1e-6)));
    const float minEv = -12.0;
    const float maxEv = 4.0;
    vec3 t = clamp((logv - minEv) / (maxEv - minEv), 0.0, 1.0);
    // Smoothstep-style contrast.
    t = t * t * (3.0 - 2.0 * t);
    return t;
}

vec3 ApplyTonemapper(vec3 hdrColor, int tonemapper) {
    vec3 x = max(hdrColor, vec3(0.0));

    if (tonemapper == TONEMAPPER_COMPRESSION) {
        x = TonemapCompression(x);
    } else if (tonemapper == TONEMAPPER_ACES) {
        x = TonemapACES(x);
    } else if (tonemapper == TONEMAPPER_AGX) {
        x = TonemapAgXApprox(x);
    } else {
        // TONEMAPPER_NONE (or unknown) performs no HDR compression.
        x = clamp(x, 0.0, 1.0);
    }

    return LinearToDisplayGamma(x);
}

#endif



// --- include end: D:\VSCode\PROJECTS\LvsEngine\content\Shaders\Utils\Tonemapping.glsl

float InterleavedGradientNoise(vec2 pixel, float frameSeed) {
    return fract(52.9829189 * fract(dot(pixel + frameSeed, vec2(0.06711056, 0.00583715))));
}

float ViewSpaceZFromReversedInfiniteDepth(float depth) {
    float nearPlane = camera.projection[3][2];
    // When using ApplyOpenGLClipDepthRemap(), the projection's reversed-infinite near plane term becomes 2*near.
    if (camera.projection[2][2] > 0.5) {
        nearPlane *= 0.5;
    }
    return -nearPlane / max(depth, 1e-6);
}

void main() {
    vec3 hdrColor = texture(sceneColor, fragUv).rgb;
    if (pushData.settings.z > 0.5) {
        vec3 glow = texture(glowColor, fragUv).rgb;
        float neonAttenuation = clamp(pushData.aoTint.a, 0.25, 4.0);
        // Shape the glow tail (affects the fade-out radius) without heavily changing the core.
        float g = max(glow.r, max(glow.g, glow.b));
        float g01 = g / (g + 0.1);
        float tailScale = pow(max(g01, 1e-6), neonAttenuation - 1.0);
        glow *= clamp(tailScale, 0.0, 32.0);
        hdrColor += glow;
    }
    float ao = texture(aoTexture, fragUv).r;
    hdrColor *= mix(pushData.aoTint.rgb, vec3(1.0), clamp(ao, 0.0, 1.0));
    vec3 color = ApplyTonemapper(hdrColor, int(pushData.settings.x + 0.5));

    if (pushData.settings.y > 0.5) {
        float n = InterleavedGradientNoise(gl_FragCoord.xy, pushData.settings.w);
        color += (n - 0.5) / 255.0;
        color = clamp(color, 0.0, 1.0);
    }

    outColor = vec4(color, 1.0);
}

