#version 450

struct BlurSettings
{
    vec4 settings;
};

uniform BlurSettings pushData;

layout(binding = 0) uniform sampler2D sourceTexture;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 texel = pushData.settings.xy * max(pushData.settings.z, 0.0);
    vec3 center = texture(sourceTexture, fragUv).xyz * 4.0;
    vec3 corners = ((texture(sourceTexture, fragUv + vec2(-texel.x, -texel.y)).xyz + texture(sourceTexture, fragUv + vec2(texel.x, -texel.y)).xyz) + texture(sourceTexture, fragUv + vec2(-texel.x, texel.y)).xyz) + texture(sourceTexture, fragUv + vec2(texel.x, texel.y)).xyz;
    outColor = vec4((center + corners) * 0.125, 1.0);
}

