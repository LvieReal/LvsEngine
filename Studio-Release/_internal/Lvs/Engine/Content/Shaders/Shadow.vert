#version 460 core

layout (location = 0) in vec3 Position;

uniform mat4 ModelMatrix;
uniform mat4 LightViewProjectionMatrix;

void main()
{
    gl_Position = LightViewProjectionMatrix * ModelMatrix * vec4(Position, 1.0);
}
