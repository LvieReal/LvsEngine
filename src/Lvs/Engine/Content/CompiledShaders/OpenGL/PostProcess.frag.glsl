#version 450

struct PostSettings
{
    vec4 settings;
};

uniform PostSettings pushData;

layout(binding = 1) uniform sampler2D sceneColor;
layout(binding = 2) uniform sampler2D glowColor;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

float InterleavedGradientNoise(vec2 pixel, float frameSeed)
{
    return fract(52.98291778564453125 * fract(dot(pixel + vec2(frameSeed), vec2(0.067110560834407806396484375, 0.005837149918079376220703125))));
}

void main()
{
    vec3 hdrColor = texture(sceneColor, fragUv).xyz;
    if (pushData.settings.z > 0.5)
    {
        hdrColor += texture(glowColor, fragUv).xyz;
    }
    vec3 color = max(hdrColor, vec3(0.0));
    if (pushData.settings.x > 0.5)
    {
        color = pow(color, vec3(0.4545454680919647216796875));
    }
    if (pushData.settings.y > 0.5)
    {
        vec2 param = gl_FragCoord.xy;
        float param_1 = pushData.settings.w;
        float n = InterleavedGradientNoise(param, param_1);
        color += vec3((n - 0.5) / 255.0);
        color = clamp(color, vec3(0.0), vec3(1.0));
    }
    outColor = vec4(color, 1.0);
}

