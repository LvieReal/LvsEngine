#version 460

in vec2 UV;

out vec4 FragColor;

uniform float Time;
uniform int GammaCorrection;
uniform sampler2D ScreenTexture;

const highp float NOISE_GRANULARITY = 0.5 / 255.0;

highp float random(highp vec2 coords)
{
   return fract(sin(dot(coords.xy + Time, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec3 color = texture(ScreenTexture, UV).rgb;

    if (GammaCorrection == 1)
        color = pow(color, vec3(1.0 / 2.2));

    color += mix(-NOISE_GRANULARITY, NOISE_GRANULARITY, random(UV));

    FragColor = vec4(color, 1.0);
}