#version 450

layout(binding = 0, std140) uniform CameraUBO
{
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 ambient;
    vec4 skyTint;
    vec4 renderSettings;
    vec4 lightingSettings;
    vec4 cameraForward;
} camera;

struct PostSettings
{
    vec4 settings;
    vec4 aoTint;
};

uniform PostSettings pushData;

layout(binding = 1) uniform sampler2D sceneColor;
layout(binding = 2) uniform sampler2D glowColor;
layout(binding = 16) uniform sampler2D aoTexture;
layout(binding = 17) uniform sampler2D shadowVolumeMask;
layout(binding = 18) uniform sampler2D depthColor;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

vec3 TonemapCompression(vec3 x)
{
    return x / (x + vec3(1.0));
}

vec3 TonemapACES(vec3 x)
{
    return clamp((x * ((x * 2.5099999904632568359375) + vec3(0.02999999932944774627685546875))) / ((x * ((x * 2.4300000667572021484375) + vec3(0.589999973773956298828125))) + vec3(0.14000000059604644775390625)), vec3(0.0), vec3(1.0));
}

vec3 TonemapAgXApprox(inout vec3 x)
{
    x = max(x, vec3(0.0));
    vec3 logv = log2(max(x, vec3(9.9999999747524270787835121154785e-07)));
    vec3 t = clamp((logv - vec3(-12.0)) / vec3(16.0), vec3(0.0), vec3(1.0));
    t = (t * t) * (vec3(3.0) - (t * 2.0));
    return t;
}

vec3 LinearToDisplayGamma(inout vec3 x)
{
    x = clamp(x, vec3(0.0), vec3(1.0));
    return pow(x, vec3(0.4545454680919647216796875));
}

vec3 ApplyTonemapper(vec3 hdrColor, int tonemapper)
{
    vec3 x = max(hdrColor, vec3(0.0));
    if (tonemapper == 2)
    {
        vec3 param = x;
        x = TonemapCompression(param);
    }
    else
    {
        if (tonemapper == 3)
        {
            vec3 param_1 = x;
            x = TonemapACES(param_1);
        }
        else
        {
            if (tonemapper == 4)
            {
                vec3 param_2 = x;
                vec3 _143 = TonemapAgXApprox(param_2);
                x = _143;
            }
            else
            {
                x = clamp(x, vec3(0.0), vec3(1.0));
            }
        }
    }
    vec3 param_3 = x;
    vec3 _151 = LinearToDisplayGamma(param_3);
    return _151;
}

float InterleavedGradientNoise(vec2 pixel, float frameSeed)
{
    return fract(52.98291778564453125 * fract(dot(pixel + vec2(frameSeed), vec2(0.067110560834407806396484375, 0.005837149918079376220703125))));
}

void main()
{
    vec3 hdrColor = texture(sceneColor, fragUv).xyz;
    if (pushData.settings.z > 0.5)
    {
        vec3 glow = texture(glowColor, fragUv).xyz;
        float neonAttenuation = clamp(pushData.aoTint.w, 0.25, 4.0);
        float g = max(glow.x, max(glow.y, glow.z));
        float g01 = g / (g + 0.100000001490116119384765625);
        float tailScale = pow(max(g01, 9.9999999747524270787835121154785e-07), neonAttenuation - 1.0);
        glow *= clamp(tailScale, 0.0, 32.0);
        hdrColor += glow;
    }
    float ao = texture(aoTexture, fragUv).x;
    hdrColor *= mix(pushData.aoTint.xyz, vec3(1.0), vec3(clamp(ao, 0.0, 1.0)));
    vec3 param = hdrColor;
    int param_1 = int(pushData.settings.x + 0.5);
    vec3 color = ApplyTonemapper(param, param_1);
    if (pushData.settings.y > 0.5)
    {
        vec2 param_2 = gl_FragCoord.xy;
        float param_3 = pushData.settings.w;
        float n = InterleavedGradientNoise(param_2, param_3);
        color += vec3((n - 0.5) / 255.0);
        color = clamp(color, vec3(0.0), vec3(1.0));
    }
    outColor = vec4(color, 1.0);
}

