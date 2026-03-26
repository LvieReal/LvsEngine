#version 450

layout(location = 0) in vec2 fragUv;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 ambient; // rgb: ambient color, a: ambient strength
    vec4 skyTint;
    vec4 renderSettings;
    vec4 lightingSettings;
    vec4 cameraForward;
} camera;

layout(set = 0, binding = 1) uniform sampler2D sceneColor;
layout(set = 0, binding = 2) uniform sampler2D glowColor;
layout(set = 0, binding = 16) uniform sampler2D aoTexture;
layout(set = 0, binding = 17) uniform sampler2D shadowVolumeMask;
layout(set = 0, binding = 18) uniform sampler2D depthColor;

layout(push_constant) uniform PostSettings {
    vec4 settings; // x: gammaEnabled, y: ditheringEnabled, z: neonEnabled, w: frameSeed
    vec4 aoTint;   // rgb: tint applied when AO factor is 0
} pushData;

layout(location = 0) out vec4 outColor;

float InterleavedGradientNoise(vec2 pixel, float frameSeed) {
    return fract(52.9829189 * fract(dot(pixel + frameSeed, vec2(0.06711056, 0.00583715))));
}

float ViewSpaceZFromReversedInfiniteDepth(float depth) {
    float nearPlane = camera.projection[3][2];
    // When using ApplyOpenGLClipDepthRemap(), the projection's reversed-infinite near plane term becomes 2*near.
    if (camera.projection[2][2] > 0.5) {
        nearPlane *= 0.5;
    }
    return -nearPlane / max(depth, 1e-6);
}

void main() {
    vec4 sceneSample = texture(sceneColor, fragUv);
    vec3 hdrColor = sceneSample.rgb;
    if (pushData.settings.z > 0.5) {
        hdrColor += texture(glowColor, fragUv).rgb;
    }
    float ao = texture(aoTexture, fragUv).r;
    hdrColor *= mix(pushData.aoTint.rgb, vec3(1.0), clamp(ao, 0.0, 1.0));
    float shadow = texture(shadowVolumeMask, fragUv).r;
    shadow = clamp(shadow, 0.0, 1.0);

    // Fade shadow volumes out by receiver distance (DirectionalLight.ShadowMaxDistance).
    float shadowMaxDist = camera.lightingSettings.y;
    float shadowFadeStart = camera.lightingSettings.z;
    if (shadowMaxDist > 1e-4 && shadowFadeStart > 0.0) {
        float depth = texture(depthColor, fragUv).r;
        float dist = -ViewSpaceZFromReversedInfiniteDepth(depth);
        float start = min(shadowFadeStart, shadowMaxDist);
        float fade = 1.0 - smoothstep(start, shadowMaxDist, dist);
        shadow *= clamp(fade, 0.0, 1.0);
    }

    // Shadow volumes only provide an occlusion mask. Instead of forcing full black,
    // keep a global ambient floor/tint based on Lighting.Ambient/AmbientStrength.
    vec3 ambientColor = max(camera.ambient.rgb, vec3(0.0));
    float ambientStrength = clamp(camera.ambient.a, 0.0, 1.0);
    float shadowScale = mix(1.0, ambientStrength, shadow);
    vec3 shadowTint = mix(vec3(1.0), ambientColor, shadow);
    hdrColor *= shadowScale * shadowTint;
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

