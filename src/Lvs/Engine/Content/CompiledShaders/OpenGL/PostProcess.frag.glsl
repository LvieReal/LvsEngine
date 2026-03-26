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

float ViewSpaceZFromReversedInfiniteDepth(float depth)
{
    float nearPlane = camera.projection[3].z;
    if (camera.projection[2].z > 0.5)
    {
        nearPlane *= 0.5;
    }
    return (-nearPlane) / max(depth, 9.9999999747524270787835121154785e-07);
}

float InterleavedGradientNoise(vec2 pixel, float frameSeed)
{
    return fract(52.98291778564453125 * fract(dot(pixel + vec2(frameSeed), vec2(0.067110560834407806396484375, 0.005837149918079376220703125))));
}

void main()
{
    vec4 sceneSample = texture(sceneColor, fragUv);
    vec3 hdrColor = sceneSample.xyz;
    if (pushData.settings.z > 0.5)
    {
        hdrColor += texture(glowColor, fragUv).xyz;
    }
    float ao = texture(aoTexture, fragUv).x;
    hdrColor *= mix(pushData.aoTint.xyz, vec3(1.0), vec3(clamp(ao, 0.0, 1.0)));
    float shadow = texture(shadowVolumeMask, fragUv).x;
    shadow = clamp(shadow, 0.0, 1.0);
    float shadowMaxDist = camera.lightingSettings.y;
    float shadowFadeStart = camera.lightingSettings.z;
    if ((shadowMaxDist > 9.9999997473787516355514526367188e-05) && (shadowFadeStart > 0.0))
    {
        float depth = texture(depthColor, fragUv).x;
        float param = depth;
        float dist = -ViewSpaceZFromReversedInfiniteDepth(param);
        float start = min(shadowFadeStart, shadowMaxDist);
        float fade = 1.0 - smoothstep(start, shadowMaxDist, dist);
        shadow *= clamp(fade, 0.0, 1.0);
    }
    vec3 ambientColor = max(camera.ambient.xyz, vec3(0.0));
    float ambientStrength = clamp(camera.ambient.w, 0.0, 1.0);
    float shadowScale = mix(1.0, ambientStrength, shadow);
    vec3 shadowTint = mix(vec3(1.0), ambientColor, vec3(shadow));
    hdrColor *= (shadowTint * shadowScale);
    vec3 color = max(hdrColor, vec3(0.0));
    if (pushData.settings.x > 0.5)
    {
        color = pow(color, vec3(0.4545454680919647216796875));
    }
    if (pushData.settings.y > 0.5)
    {
        vec2 param_1 = gl_FragCoord.xy;
        float param_2 = pushData.settings.w;
        float n = InterleavedGradientNoise(param_1, param_2);
        color += vec3((n - 0.5) / 255.0);
        color = clamp(color, vec3(0.0), vec3(1.0));
    }
    outColor = vec4(color, 1.0);
}

