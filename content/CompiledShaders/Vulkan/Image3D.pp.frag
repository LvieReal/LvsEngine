#version 450

layout(set = 1, binding = 1) uniform sampler2D imageTex;

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 tex = texture(imageTex, fragUv);
    vec4 color = tex * fragColor;
    if (color.a <= 0.001) {
        discard;
    }
    outColor = color;
}

