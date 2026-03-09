#version 450

layout(location = 0) in vec3 texCoord;

layout(set = 0, binding = 1) uniform samplerCube skyboxTex;
layout(push_constant) uniform SkyPush {
    mat4 viewProjection;
    vec4 tint;
} skyPush;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = texture(skyboxTex, normalize(texCoord)).rgb * skyPush.tint.rgb;
    outColor = vec4(color, 1.0);
}
