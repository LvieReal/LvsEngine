#version 450

layout(location = 0) in vec2 fragUv;

layout(set = 0, binding = 0) uniform sampler2D sourceTexture;

layout(push_constant) uniform BlurSettings {
    vec4 settings; // xy: texel size, z: offset, w: unused
} pushData;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 texel = pushData.settings.xy * max(pushData.settings.z, 0.0);
    vec3 center = texture(sourceTexture, fragUv).rgb * 4.0;
    vec3 corners =
        texture(sourceTexture, fragUv + vec2(-texel.x, -texel.y)).rgb +
        texture(sourceTexture, fragUv + vec2(texel.x, -texel.y)).rgb +
        texture(sourceTexture, fragUv + vec2(-texel.x, texel.y)).rgb +
        texture(sourceTexture, fragUv + vec2(texel.x, texel.y)).rgb;

    outColor = vec4((center + corners) * 0.125, 1.0);
}
