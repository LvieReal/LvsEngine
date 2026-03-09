#version 450

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform ShadowPushConstants {
    mat4 lightViewProjection;
    mat4 model;
} pushData;

void main() {
    gl_Position = pushData.lightViewProjection * pushData.model * vec4(inPosition, 1.0);
}
