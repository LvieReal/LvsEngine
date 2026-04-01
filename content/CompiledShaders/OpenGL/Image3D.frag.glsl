#version 450

struct PushConstants
{
    mat4 model;
    vec4 color;
    vec4 options;
    vec4 outlineColor;
    vec4 outlineParams;
};

uniform PushConstants pushData;

layout(binding = 1) uniform sampler2D imageTex;

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 tex = texture(imageTex, fragUv);
    vec4 color = tex * fragColor;
    bool negateMask = pushData.options.x > 0.5;
    bool depthOnly = pushData.options.y > 0.5;
    bool outlineEnabled = pushData.options.z > 0.5;
    float alphaThreshold = clamp(pushData.outlineParams.y, 0.0, 1.0);
    if (negateMask)
    {
        float mask = clamp(color.w, 0.0, 1.0);
        if (mask <= 0.001000000047497451305389404296875)
        {
            discard;
        }
        outColor = vec4(mask);
        return;
    }
    if (depthOnly)
    {
        if (color.w < alphaThreshold)
        {
            discard;
        }
        outColor = vec4(0.0);
        return;
    }
    bool _92 = !outlineEnabled;
    bool _98;
    if (_92)
    {
        _98 = color.w <= 0.001000000047497451305389404296875;
    }
    else
    {
        _98 = _92;
    }
    if (_98)
    {
        discard;
    }
    vec4 outCol = color;
    bool _111;
    if (outlineEnabled)
    {
        _111 = pushData.outlineColor.w > 0.001000000047497451305389404296875;
    }
    else
    {
        _111 = outlineEnabled;
    }
    bool _117;
    if (_111)
    {
        _117 = pushData.outlineParams.x > 0.0;
    }
    else
    {
        _117 = _111;
    }
    if (_117)
    {
        int radius = int(pushData.outlineParams.x);
        ivec2 ts = textureSize(imageTex, 0);
        ivec2 base = ivec2(fragUv * vec2(ts));
        float aMax = 0.0;
        int _141 = -radius;
        for (int y = _141; y <= radius; y++)
        {
            int _152 = -radius;
            for (int x = _152; x <= radius; x++)
            {
                if ((x == 0) && (y == 0))
                {
                    continue;
                }
                aMax = max(aMax, texelFetch(imageTex, base + ivec2(x, y), 0).w);
            }
        }
        float inside = smoothstep(alphaThreshold - 0.0199999995529651641845703125, alphaThreshold + 0.0199999995529651641845703125, color.w);
        float neighbor = smoothstep(alphaThreshold - 0.0199999995529651641845703125, alphaThreshold + 0.0199999995529651641845703125, aMax);
        float outlineMask = (1.0 - inside) * neighbor;
        outCol = mix(outCol, vec4(pushData.outlineColor.xyz, pushData.outlineColor.w), vec4((1.0 - inside) * neighbor));
    }
    outColor = outCol;
}

