#version 460

layout(set = 0, binding = 0) uniform CameraUniformDynamic {
    mat4 view;
    mat4 view_inv;
    mat4 proj;
    mat4 proj_inv;
} camera;

layout(set = 1, binding = 0) uniform MeshUniformDynamic {
    mat4 model;
} mesh;

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexTextureCoord;
layout(location = 3) in vec3 vertexTangent;
layout(location = 4) in vec3 vertexBinormal;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outTangent;
layout(location = 3) out vec3 outBinormal;
layout(location = 4) out mat3 outNormalMatrix;

void main() {
    outNormalMatrix = transpose(inverse(mat3(mesh.model)));
    outTexCoord = vertexTextureCoord;

    outNormal = normalize(vertexNormal);
    outTangent = normalize(vertexTangent);
    outBinormal = normalize(vertexBinormal);

    mat4 model = mat4(0.01);

    gl_Position = camera.proj * camera.view * model * vec4(vertexPosition, 1.0);
}
