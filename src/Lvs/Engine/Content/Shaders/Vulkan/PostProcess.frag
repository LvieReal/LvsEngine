#version 450

layout(location = 0) in vec2 fragUv;

layout(set = 0, binding = 0) uniform sampler2D sceneColor;
layout(set = 0, binding = 1) uniform sampler2D glowColor;

layout(push_constant) uniform PostSettings {
    vec4 settings; // x: gammaEnabled, y: ditheringEnabled, z: frameSeed, w: unused
} pushData;

layout(location = 0) out vec4 outColor;

float InterleavedGradientNoise(vec2 pixel, float frameSeed) {
    return fract(52.9829189 * fract(dot(pixel + frameSeed, vec2(0.06711056, 0.00583715))));
}

void main() {
    vec3 hdrColor = texture(sceneColor, fragUv).rgb + (texture(glowColor, fragUv).rgb);
    vec3 color = hdrColor / (vec3(1.0) + hdrColor);

    if (pushData.settings.x > 0.5) {
        color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
    }

    if (pushData.settings.y > 0.5) {
        float n = InterleavedGradientNoise(gl_FragCoord.xy, pushData.settings.z);
        color += (n - 0.5) / 255.0;
        color = clamp(color, 0.0, 1.0);
    }

    outColor = vec4(color, 1.0);
}
