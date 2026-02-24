#version 460 core

layout (location = 0) in vec3 Position;

out vec3 TexCoord;

uniform mat4 ViewMatrix;
uniform mat4 ProjectionMatrix;

void main()
{
    TexCoord = Position;
    vec4 pos = ProjectionMatrix * ViewMatrix * vec4(Position, 1.0);
    gl_Position = vec4(pos.xy, 0.0, pos.w);
}