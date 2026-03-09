#version 450

struct SkyPush
{
    mat4 viewProjection;
    vec4 tint;
};

uniform SkyPush skyPush;

layout(binding = 1) uniform samplerCube skyboxTex;

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 texCoord;

void main()
{
    outColor = texture(skyboxTex, normalize(texCoord)) * vec4(skyPush.tint.xyz, 1.0);
}

