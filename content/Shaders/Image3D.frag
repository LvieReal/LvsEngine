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

    if (negateMask)
    {
        // Shader outputs a mask in RGB/A; blending performs the masked inversion.
        const float mask = clamp(color.a, 0.0, 1.0);
        if (mask <= 0.001)
            discard;
        outColor = vec4(mask, mask, mask, mask);
        return;
    }

    if (depthOnly)
    {
        // Depth mask prepass: only write depth for sufficiently covered pixels.
        if (color.a < alphaThreshold)
            discard;
        outColor = vec4(0.0);
        return;
    }

    // Snap near-opaque alpha to fully opaque to avoid subtle background bleed (most visible on dark images).
    if (color.a > 0.999)
        color.a = 1.0;

    if (!outlineEnabled && color.a <= 0.001)
        discard;

    vec4 outCol = color;

    if (outlineEnabled && pushData.outlineColor.a > 0.001 && pushData.outlineParams.x > 0.0)
    {
        int radius = int(pushData.outlineParams.x);

        ivec2 ts = textureSize(imageTex, 0);
        ivec2 base = ivec2(fragUv * vec2(ts));

        float aMax = 0.0;

        for (int y = -radius; y <= radius; y++)
        {
            for (int x = -radius; x <= radius; x++)
            {
                if (x == 0 && y == 0)
                    continue;

                aMax = max(aMax, texelFetch(imageTex, base + ivec2(x, y), 0).a);
            }
        }

        float inside = smoothstep(alphaThreshold - 0.02, alphaThreshold + 0.02, color.a);
        float neighbor = smoothstep(alphaThreshold - 0.02, alphaThreshold + 0.02, aMax);

        float outlineMask = (1.0 - inside) * neighbor;

        outCol = mix(outCol, vec4(pushData.outlineColor.rgb, pushData.outlineColor.a), (1.0 - inside) * neighbor);
    }

    outColor = outCol;
}