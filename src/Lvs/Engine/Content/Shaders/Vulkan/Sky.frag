#version 450

layout(location = 0) in vec3 texCoord;

layout(set = 0, binding = 0) uniform samplerCube skyboxTex;
layout(push_constant) uniform SkyPush {
    mat4 viewProjection;
    vec4 tint;
} skyPush;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(skyboxTex, normalize(texCoord)) * vec4(skyPush.tint.rgb, 1.0);
}
