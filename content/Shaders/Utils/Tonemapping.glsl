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

