#ifndef LVS_TONEMAPPING_GLSL
#define LVS_TONEMAPPING_GLSL

const int TONEMAPPER_NONE = 1;
const int TONEMAPPER_COMPRESSION = 2;
const int TONEMAPPER_ACES = 3;
const int TONEMAPPER_AGX = 4;

vec3 Saturate(vec3 v) {
    return clamp(v, 0.0, 1.0);
}

vec3 LinearToDisplayGamma(vec3 x) {
    return pow(Saturate(x), vec3(1.0 / 2.2));
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

vec3 AgXCurve3(vec3 v) {
    const float threshold = 0.6060606060606061;
    const float a_up = 69.86278913545539;
    const float a_down = 59.507875;
    const float b_up = 13.0 / 4.0;
    const float b_down = 3.0 / 1.0;
    const float c_up = -4.0 / 13.0;
    const float c_down = -1.0 / 3.0;

    vec3 mask = step(v, vec3(threshold));
    vec3 a = a_up + (a_down - a_up) * mask;
    vec3 b = b_up + (b_down - b_up) * mask;
    vec3 c = c_up + (c_down - c_up) * mask;
    return 0.5 + (((-2.0 * threshold)) + 2.0 * v) * pow(1.0 + a * pow(abs(v - threshold), b), c);
}

vec3 TonemapAgX(vec3 /*Linear BT.709*/ci) {
    const float min_ev = -12.473931188332413;
    const float max_ev = 4.026068811667588;
    const float dynamic_range = max_ev - min_ev;

    const mat3 agx_mat = mat3(
        0.8424010709504686, 0.04240107095046854, 0.04240107095046854,
        0.07843650156180276, 0.8784365015618028, 0.07843650156180276,
        0.0791624274877287, 0.0791624274877287, 0.8791624274877287
    );
    const mat3 agx_mat_inv = mat3(
        1.1969986613119143, -0.053001338688085674, -0.053001338688085674,
        -0.09804562695225345, 1.1519543730477466, -0.09804562695225345,
        -0.09895303435966087, -0.09895303435966087, 1.151046965640339
    );

    // Input transform (inset)
    ci = agx_mat * ci;

    // Apply sigmoid function
    vec3 ct = Saturate(log2(ci) * (1.0 / dynamic_range) - (min_ev / dynamic_range));
    vec3 co = AgXCurve3(ct);

    // Inverse input transform (outset)
    co = agx_mat_inv * co;

    return /*BT.709 (NOT linear)*/co;
}

vec3 ApplyTonemapper(vec3 hdrColor, int tonemapper) {
    vec3 x = max(hdrColor, vec3(0.0));

    if (tonemapper == TONEMAPPER_COMPRESSION) {
        x = TonemapCompression(x);
    } else if (tonemapper == TONEMAPPER_ACES) {
        x = TonemapACES(x);
    } else if (tonemapper == TONEMAPPER_AGX) {
        x = TonemapAgX(x);
    } else {
        // TONEMAPPER_NONE (or unknown) performs no HDR compression.
        x = clamp(x, 0.0, 1.0);
    }

    return LinearToDisplayGamma(x);
}

#endif