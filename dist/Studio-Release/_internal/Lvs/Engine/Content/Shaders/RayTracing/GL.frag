#version 460

in vec2 UV;

out vec4 FragColor;

uniform sampler2D ScreenTexture;

void main()
{
    FragColor = texture(ScreenTexture, UV);
}