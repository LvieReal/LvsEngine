#version 450

layout(set = 1, binding = 1) uniform sampler2D imageTex;

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec4 fragColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    vec4 options; // x: negateMask, y: depthOnly, z: outlineEnabled
    vec4 outlineColor; // rgb color, a alpha
    vec4 outlineParams; // x thicknessPixels, y alphaThreshold
} pushData;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 tex = texture(imageTex, fragUv);
    vec4 color = tex * fragColor;
    const bool negateMask = pushData.options.x > 0.5;
    const bool depthOnly = pushData.options.y > 0.5;
    const bool outlineEnabled = pushData.options.z > 0.5;
    const float alphaThreshold = clamp(pushData.outlineParams.y, 0.0, 1.0);

    if (negateMask) {
        // Shader outputs a mask in RGB/A; blending performs the masked inversion.
        const float mask = clamp(color.a, 0.0, 1.0);
        if (mask <= 0.001) {
            discard;
        }
        outColor = vec4(mask, mask, mask, mask);
        return;
    }

    if (depthOnly) {
        // Depth mask prepass: only write depth for sufficiently covered pixels.
        if (color.a < alphaThreshold) {
            discard;
        }
        outColor = vec4(0.0);
        return;
    }

    if (color.a <= 0.001) {
        discard;
    }

    vec4 outCol = color;
    if (outlineEnabled && pushData.outlineColor.a > 0.001 && pushData.outlineParams.x > 0.0) {
        // Outside outline based on alpha coverage.
        ivec2 ts = textureSize(imageTex, 0);
        vec2 invSize = vec2(1.0) / vec2(max(ts, ivec2(1)));
        float px = max(0.5, pushData.outlineParams.x);
        vec2 o = invSize * px;

        float a0 = color.a;
        float aMax = 0.0;
        aMax = max(aMax, texture(imageTex, fragUv + vec2( o.x, 0.0)).a * fragColor.a);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(-o.x, 0.0)).a * fragColor.a);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(0.0,  o.y)).a * fragColor.a);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(0.0, -o.y)).a * fragColor.a);
        aMax = max(aMax, texture(imageTex, fragUv + vec2( o.x,  o.y)).a * fragColor.a);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(-o.x,  o.y)).a * fragColor.a);
        aMax = max(aMax, texture(imageTex, fragUv + vec2( o.x, -o.y)).a * fragColor.a);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(-o.x, -o.y)).a * fragColor.a);

        float inside = step(alphaThreshold, a0);
        float neighborInside = step(alphaThreshold, aMax);
        float outlineMask = (1.0 - inside) * neighborInside;

        if (outlineMask > 0.0) {
            vec4 oc = vec4(pushData.outlineColor.rgb, pushData.outlineColor.a * outlineMask);
            outCol = oc;
        }
    }

    outColor = outCol;
}

