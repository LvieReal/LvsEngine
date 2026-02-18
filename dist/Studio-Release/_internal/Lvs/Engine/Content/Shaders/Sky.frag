#version 460 core

in vec3 TexCoord;

out vec4 FragColor;

uniform samplerCube Skybox;
uniform vec3 Tint;

void main()
{
    FragColor = texture(Skybox, TexCoord) * vec4(Tint, 1.0);
}