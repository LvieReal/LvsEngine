#version 450

layout(location = 0) in vec2 fragUv;

layout(set = 0, binding = 1) uniform sampler2D sceneColor;
layout(set = 0, binding = 2) uniform sampler2D glowColor;
layout(set = 0, binding = 16) uniform sampler2D aoTexture;

layout(push_constant) uniform PostSettings {
    vec4 settings; // x: gammaEnabled, y: ditheringEnabled, z: neonEnabled, w: frameSeed
    vec4 aoTint;   // rgb: tint applied when AO factor is 0
} pushData;

layout(location = 0) out vec4 outColor;

float InterleavedGradientNoise(vec2 pixel, float frameSeed) {
    return fract(52.9829189 * fract(dot(pixel + frameSeed, vec2(0.06711056, 0.00583715))));
}

void main() {
    vec3 hdrColor = texture(sceneColor, fragUv).rgb;
    if (pushData.settings.z > 0.5) {
        hdrColor += texture(glowColor, fragUv).rgb;
    }
    float ao = texture(aoTexture, fragUv).r;
    hdrColor *= mix(pushData.aoTint.rgb, vec3(1.0), clamp(ao, 0.0, 1.0));
    vec3 color = max(hdrColor, vec3(0.0));

    if (pushData.settings.x > 0.5) {
        color = pow(color, vec3(1.0 / 2.2));
    }

    if (pushData.settings.y > 0.5) {
        float n = InterleavedGradientNoise(gl_FragCoord.xy, pushData.settings.w);
        color += (n - 0.5) / 255.0;
        color = clamp(color, 0.0, 1.0);
    }

    outColor = vec4(color, 1.0);
}
