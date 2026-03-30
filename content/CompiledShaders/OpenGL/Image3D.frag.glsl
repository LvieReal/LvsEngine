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
    if (color.w <= 0.001000000047497451305389404296875)
    {
        discard;
    }
    vec4 outCol = color;
    bool _106;
    if (outlineEnabled)
    {
        _106 = pushData.outlineColor.w > 0.001000000047497451305389404296875;
    }
    else
    {
        _106 = outlineEnabled;
    }
    bool _112;
    if (_106)
    {
        _112 = pushData.outlineParams.x > 0.0;
    }
    else
    {
        _112 = _106;
    }
    if (_112)
    {
        ivec2 ts = textureSize(imageTex, 0);
        vec2 invSize = vec2(1.0) / vec2(max(ts, ivec2(1)));
        float px = max(0.5, pushData.outlineParams.x);
        vec2 o = invSize * px;
        float a0 = color.w;
        float aMax = 0.0;
        aMax = max(aMax, texture(imageTex, fragUv + vec2(o.x, 0.0)).w * fragColor.w);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(-o.x, 0.0)).w * fragColor.w);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(0.0, o.y)).w * fragColor.w);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(0.0, -o.y)).w * fragColor.w);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(o.x, o.y)).w * fragColor.w);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(-o.x, o.y)).w * fragColor.w);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(o.x, -o.y)).w * fragColor.w);
        aMax = max(aMax, texture(imageTex, fragUv + vec2(-o.x, -o.y)).w * fragColor.w);
        float inside = step(alphaThreshold, a0);
        float neighborInside = step(alphaThreshold, aMax);
        float outlineMask = (1.0 - inside) * neighborInside;
        if (outlineMask > 0.0)
        {
            vec4 oc = vec4(pushData.outlineColor.xyz, pushData.outlineColor.w * outlineMask);
            outCol = oc;
        }
    }
    outColor = outCol;
}

