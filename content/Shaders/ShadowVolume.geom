#version 450

layout(triangles_adjacency) in;
layout(triangle_strip, max_vertices = 18) out;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 ambient;
    vec4 skyTint;
    vec4 renderSettings;
    vec4 lightingSettings;
    vec4 cameraForward;
} camera;

layout(push_constant) uniform ShadowVolumePush {
    uvec4 data;          // x: base instance
    vec4 lightDirExtrude; // xyz: light ray direction (world), w: extrude distance
    vec4 params;         // x: bias (world units)
} pushData;

layout(location = 0) in vec3 vWorldPos[];

vec4 ToClip(vec3 worldPos) {
    return camera.projection * camera.view * vec4(worldPos, 1.0);
}

float FacingToLight(vec3 a, vec3 b, vec3 c, vec3 lightRayDir) {
    vec3 n = cross(b - a, c - a);
    return sign(dot(n, -lightRayDir));
}

void EmitTriClip(vec4 a, vec4 b, vec4 c) {
    gl_Position = a;
    EmitVertex();
    gl_Position = b;
    EmitVertex();
    gl_Position = c;
    EmitVertex();
    EndPrimitive();
}

void EmitQuadClip(vec4 a, vec4 b, vec4 b2, vec4 a2) {
    gl_Position = a;
    EmitVertex();
    gl_Position = b;
    EmitVertex();
    gl_Position = a2;
    EmitVertex();
    gl_Position = b2;
    EmitVertex();
    EndPrimitive();
}

void main() {
    vec3 v0 = vWorldPos[0];
    vec3 a0 = vWorldPos[1];
    vec3 v1 = vWorldPos[2];
    vec3 a1 = vWorldPos[3];
    vec3 v2 = vWorldPos[4];
    vec3 a2 = vWorldPos[5];

    vec3 lightRayDir = normalize(pushData.lightDirExtrude.xyz);
    float extrudeDist = max(pushData.lightDirExtrude.w, 1.0);
    float bias = pushData.params.x;

    float f = FacingToLight(v0, v1, v2, lightRayDir);
    bool frontFacing = f > 0.0;

    vec3 bv0 = v0;
    vec3 bv1 = v1;
    vec3 bv2 = v2;
    if (abs(bias) > 0.0)
    {
        vec3 off = -lightRayDir * bias;
        bv0 += off;
        bv1 += off;
        bv2 += off;
    }

    vec3 e0 = bv0 + (lightRayDir * extrudeDist);
    vec3 e1 = bv1 + (lightRayDir * extrudeDist);
    vec3 e2 = bv2 + (lightRayDir * extrudeDist);

    vec4 c0 = ToClip(bv0);
    vec4 c1 = ToClip(bv1);
    vec4 c2 = ToClip(bv2);

    // NOTE: engine uses a reversed-infinite projection (Math::Projection::ReversedInfinitePerspective).
    // Finite extrusion creates a visible "end cap" plane at range; instead, build vertices at infinity
    // by projecting the light direction with w=0 (translation-free). This yields correct depth=0 for Vulkan
    // and depth=0 for OpenGL after ApplyOpenGLClipDepthRemap().
    vec4 infClip = camera.projection * camera.view * vec4(lightRayDir, 0.0);
    bool useInfiniteExtrusion = infClip.w > 1e-5;

    vec4 ce0 = useInfiniteExtrusion ? infClip : ToClip(e0);
    vec4 ce1 = useInfiniteExtrusion ? infClip : ToClip(e1);
    vec4 ce2 = useInfiniteExtrusion ? infClip : ToClip(e2);

    int capMode = int(pushData.params.y + 0.5);
    bool emitNearCap = false;
    bool emitFarCap = false;
    if (capMode == 0) {
        // FrontNear_BackFar (legacy)
        emitNearCap = frontFacing;
        emitFarCap = !frontFacing;
    } else if (capMode == 1) {
        // BackNear_FrontFar
        emitNearCap = !frontFacing;
        emitFarCap = frontFacing;
    } else {
        // None
        emitNearCap = false;
        emitFarCap = false;
    }
    if (useInfiniteExtrusion) {
        // Far cap degenerates in infinite-extrusion mode; the sides close at infinity.
        emitFarCap = false;
    }

    if (emitNearCap) {
        EmitTriClip(c0, c1, c2);
    }
    if (emitFarCap) {
        // Far cap (extruded, reverse winding).
        EmitTriClip(ce2, ce1, ce0);
    }

    if (frontFacing) {
        // Silhouette edges: current is front-facing, adjacent is back-facing or missing.
        bool missing0 = (distance(a0, v0) < 1e-7) || (distance(a0, v1) < 1e-7);
        bool missing1 = (distance(a1, v1) < 1e-7) || (distance(a1, v2) < 1e-7);
        bool missing2 = (distance(a2, v2) < 1e-7) || (distance(a2, v0) < 1e-7);

        float af0 = missing0 ? -1.0 : FacingToLight(v1, v0, a0, lightRayDir);
        float af1 = missing1 ? -1.0 : FacingToLight(v2, v1, a1, lightRayDir);
        float af2 = missing2 ? -1.0 : FacingToLight(v0, v2, a2, lightRayDir);

        if (af0 <= 0.0) {
            EmitQuadClip(c0, c1, ce1, ce0);
        }
        if (af1 <= 0.0) {
            EmitQuadClip(c1, c2, ce2, ce1);
        }
        if (af2 <= 0.0) {
            EmitQuadClip(c2, c0, ce0, ce2);
        }
    }
}
