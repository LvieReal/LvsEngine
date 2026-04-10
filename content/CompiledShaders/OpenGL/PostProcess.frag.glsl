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
layout(binding = 17) uniform sampler2D depthColor;

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

vec3 Saturate(vec3 v)
{
    return clamp(v, vec3(0.0), vec3(1.0));
}

vec3 AgXCurve3(vec3 v)
{
    vec3 mask = step(v, vec3(0.60606062412261962890625));
    vec3 a = vec3(69.86278533935546875) + (mask * (-10.3549137115478515625));
    vec3 b = vec3(3.25) + (mask * (-0.25));
    vec3 c = vec3(-0.3076923191547393798828125) + (mask * (-0.02564102597534656524658203125));
    return vec3(0.5) + ((vec3(-1.2121212482452392578125) + (v * 2.0)) * pow(vec3(1.0) + (a * pow(abs(v - vec3(0.60606062412261962890625)), b)), c));
}

vec3 TonemapAgX(inout vec3 ci)
{
    ci = mat3(vec3(0.842401087284088134765625, 0.0424010716378688812255859375, 0.0424010716378688812255859375), vec3(0.078436501324176788330078125, 0.878436505794525146484375, 0.078436501324176788330078125), vec3(0.079162426292896270751953125, 0.079162426292896270751953125, 0.87916243076324462890625)) * ci;
    vec3 param = (log2(ci) * 0.060606062412261962890625) - vec3(-0.755995810031890869140625);
    vec3 ct = Saturate(param);
    vec3 param_1 = ct;
    vec3 co = AgXCurve3(param_1);
    co = mat3(vec3(1.19699871540069580078125, -0.0530013404786586761474609375, -0.0530013404786586761474609375), vec3(-0.098045624792575836181640625, 1.1519544124603271484375, -0.098045624792575836181640625), vec3(-0.098953031003475189208984375, -0.098953031003475189208984375, 1.1510469913482666015625)) * co;
    return co;
}

vec3 LinearToDisplayGamma(vec3 x)
{
    vec3 param = x;
    return pow(Saturate(param), vec3(0.4545454680919647216796875));
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
                vec3 _213 = TonemapAgX(param_2);
                x = _213;
            }
            else
            {
                x = clamp(x, vec3(0.0), vec3(1.0));
            }
        }
    }
    vec3 param_3 = x;
    return LinearToDisplayGamma(param_3);
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

