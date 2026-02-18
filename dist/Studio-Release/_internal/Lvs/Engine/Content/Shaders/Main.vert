#version 460 core

layout (location = 0) in vec3 Position;
layout (location = 1) in vec3 Normal;

out vec3 FragColor;
out vec3 FragWorldPos;
out vec3 FragNormal;
out vec3 FragLocalPos;
out vec3 FragLocalNormal;

uniform mat4 ModelMatrix;
uniform mat4 ViewMatrix;
uniform mat4 ProjectionMatrix;
uniform vec3 CameraPosition;

uniform vec3 MeshColor;

uniform vec3 AmbientColor;
uniform float AmbientStrength;
uniform vec3 LightDirection;
uniform vec3 LightColor;
uniform float SpecularStrength;
uniform float Shininess;

void main()
{
    vec3 worldPos = vec3(ModelMatrix * vec4(Position, 1.0));
    mat3 normalMatrix = mat3(transpose(inverse(ModelMatrix)));

    vec3 N = normalize(normalMatrix * Normal);
    vec3 L = normalize(-LightDirection);
    vec3 V = normalize(CameraPosition - worldPos);
    vec3 H = normalize(L + V);

    vec3 lighting = (AmbientColor * AmbientStrength) * LightColor;

    float diffuse = max(dot(N, L), 0.0);
    lighting += diffuse * LightColor;

    float specular = pow(max(dot(N, H), 0.0), Shininess);
    lighting += SpecularStrength * specular * LightColor;

    FragColor = MeshColor * lighting;

    FragWorldPos = worldPos;
    FragNormal = N;
    FragLocalPos = Position;
    FragLocalNormal = Normal;

    gl_Position = ProjectionMatrix * ViewMatrix * vec4(worldPos, 1.0);
}