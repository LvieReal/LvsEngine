#version 450

struct SkyPush
{
    mat4 viewProjection;
    vec4 tint;
};

uniform SkyPush skyPush;

layout(location = 0) out vec3 texCoord;
layout(location = 0) in vec3 inPosition;

void main()
{
    texCoord = inPosition;
    vec4 pos = skyPush.viewProjection * vec4(inPosition, 1.0);
    gl_Position = vec4(pos.xy, 0.0, pos.w);
}

