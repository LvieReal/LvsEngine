#version 450

struct SkyPush
{
    mat4 viewProjection;
    vec4 tint;
};

uniform SkyPush skyPush;

layout(binding = 1) uniform samplerCube skyboxTex;

layout(location = 0) in vec3 texCoord;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 color = texture(skyboxTex, normalize(texCoord)).xyz * skyPush.tint.xyz;
    outColor = vec4(color, 1.0);
}

