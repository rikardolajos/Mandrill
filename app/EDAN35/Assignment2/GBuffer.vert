#version 460

layout(set = 0, binding = 0) uniform CameraUniforms {
    mat4 view;
    mat4 view_inv;
    mat4 proj;
    mat4 proj_inv;
} camera;

layout(set = 0, binding = 1) uniform MeshUniforms {
    mat4 model;
} mesh;

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexTextureCoord;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;

void main() {
    mat3 normalMatrix = transpose(inverse(mat3(mesh.model)));

    outWorldPos = vec3(mesh.model * vec4(vertexPosition, 1.0));
    outNormal = normalMatrix * normalize(vertexNormal);
    outTexCoord = vertexTextureCoord;

    gl_Position = camera.proj * camera.view * mesh.model * vec4(vertexPosition, 1.0);
}
