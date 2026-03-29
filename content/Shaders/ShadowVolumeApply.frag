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

layout(location = 0) out vec4 outSceneMul;
layout(location = 1) out vec4 outGlowMul;
layout(location = 2) out vec4 outDepthMul;
layout(location = 3) out vec4 outShadowMul;

void main() {
    // Shadow volumes provide a binary "in shadow" classification via stencil.
    // We apply a multiplicative attenuation in a fullscreen pass with a stencil test,
    // and keep other MRT attachments unchanged by outputting 1.
    vec3 ambientColor = max(camera.ambient.rgb, vec3(0.0));
    float ambientStrength = clamp(camera.ambient.a, 0.0, 1.0);

    vec3 factor = ambientColor * ambientStrength;
    outSceneMul = vec4(factor, 1.0);
    outGlowMul = vec4(1.0);
    outDepthMul = vec4(1.0);
    outShadowMul = vec4(1.0);
}

