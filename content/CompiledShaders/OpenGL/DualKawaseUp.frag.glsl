#version 450

struct BlurSettings
{
    vec4 settings;
};

uniform BlurSettings pushData;

layout(binding = 1) uniform sampler2D sourceTexture;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 texel = pushData.settings.xy * max(pushData.settings.z, 0.0);
    vec3 diagonals = ((texture(sourceTexture, fragUv + vec2(-texel.x, -texel.y)).xyz + texture(sourceTexture, fragUv + vec2(texel.x, -texel.y)).xyz) + texture(sourceTexture, fragUv + vec2(-texel.x, texel.y)).xyz) + texture(sourceTexture, fragUv + vec2(texel.x, texel.y)).xyz;
    vec3 cardinals = ((texture(sourceTexture, fragUv + vec2(-texel.x, 0.0)).xyz + texture(sourceTexture, fragUv + vec2(texel.x, 0.0)).xyz) + texture(sourceTexture, fragUv + vec2(0.0, -texel.y)).xyz) + texture(sourceTexture, fragUv + vec2(0.0, texel.y)).xyz;
    outColor = vec4((diagonals + (cardinals * 2.0)) / vec3(12.0), 1.0);
}

