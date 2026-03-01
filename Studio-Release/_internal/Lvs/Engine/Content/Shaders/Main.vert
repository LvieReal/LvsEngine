#version 460 core

layout (location = 0) in vec3 Position;
layout (location = 1) in vec3 Normal;

out vec3 FragColor;
out vec3 FragDirectionalColor;
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
uniform float LightIntensity;
uniform float SpecularStrength;
uniform float Shininess;
uniform int IgnoreDiffuseSpecular;
uniform int NumLights;
uniform int LightingPass;

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT       1

#define LIGHTING_PASS_FULL     0
#define LIGHTING_PASS_AMBIENT  1
#define LIGHTING_PASS_DIRECT   2

struct LightData
{
    float light_type;
    float color_r;
    float color_g;
    float color_b;
    float dir_or_pos_x;
    float dir_or_pos_y;
    float dir_or_pos_z;
    float reserved0;
    float intensity;
    float specular_strength;
    float shininess;
    float reserved1;
    float reserved2;
    float reserved3;
    float reserved4;
    float reserved5;
};

layout (std430, binding = 6) buffer LightBuffer
{
    LightData lights[];
};

void main()
{
    vec3 worldPos = vec3(ModelMatrix * vec4(Position, 1.0));
    mat3 normalMatrix = mat3(transpose(inverse(ModelMatrix)));

    vec3 N = normalize(normalMatrix * Normal);

    vec3 lighting;
    vec3 directionalLighting;

    if (IgnoreDiffuseSpecular == 0)
    {
        lighting = vec3(0.0);
        directionalLighting = vec3(0.0);
        vec3 firstDirectionalLighting = vec3(0.0);
        bool hasFirstDirectional = false;
        bool matchedPrimaryDirectional = false;
        float primaryDirLength = length(LightDirection);
        vec3 primaryLightDir = (primaryDirLength > 1e-6) ? normalize(-LightDirection) : vec3(0.0);

        if (LightingPass != LIGHTING_PASS_DIRECT)
        {
            lighting += (AmbientColor * AmbientStrength);
        }

        if (LightingPass != LIGHTING_PASS_AMBIENT && NumLights > 0)
        {
            for (int i = 0; i < NumLights; i++)
            {
                int lt = int(lights[i].light_type);
                if (lt == LIGHT_TYPE_DIRECTIONAL)
                {
                    vec3 lDir = normalize(-vec3(lights[i].dir_or_pos_x, lights[i].dir_or_pos_y, lights[i].dir_or_pos_z));
                    vec3 lColor = vec3(lights[i].color_r, lights[i].color_g, lights[i].color_b);
                    float lIntensity = lights[i].intensity;
                    float lSpecStr = lights[i].specular_strength;
                    float lShininess = lights[i].shininess;
                    if (lShininess <= 0.0) lShininess = 32.0;

                    vec3 V = normalize(CameraPosition - worldPos);
                    vec3 H = normalize(lDir + V);

                    float diffuse = max(dot(N, lDir), 0.0);
                    float specular = pow(max(dot(N, H), 0.0), lShininess);
                    vec3 directionalContribution = (diffuse * lColor * lIntensity) + (lSpecStr * specular * lColor * lIntensity);

                    lighting += directionalContribution;
                    if (!hasFirstDirectional)
                    {
                        firstDirectionalLighting = directionalContribution;
                        hasFirstDirectional = true;
                    }

                    if (primaryDirLength > 1e-6 && dot(lDir, primaryLightDir) >= 0.999)
                    {
                        directionalLighting += directionalContribution;
                        matchedPrimaryDirectional = true;
                    }
                }
            }
        }

        if (!matchedPrimaryDirectional && hasFirstDirectional)
        {
            directionalLighting = firstDirectionalLighting;
        }
    }
    else
    {
        lighting = vec3(1.0);
        directionalLighting = vec3(0.0);
    }

    FragColor = MeshColor * lighting;
    FragDirectionalColor = MeshColor * directionalLighting;

    FragWorldPos = worldPos;
    FragNormal = N;
    FragLocalPos = Position;
    FragLocalNormal = Normal;

    gl_Position = ProjectionMatrix * ViewMatrix * vec4(worldPos, 1.0);
}
