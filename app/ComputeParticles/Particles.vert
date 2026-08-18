#version 460

struct Particle {
    vec4 position; // xyz = position, w unused
    vec4 velocity; // xyz = velocity, w unused
};

layout(set = 0, binding = 0) uniform CameraUniformDynamic {
    mat4 view;
    mat4 view_inv;
    mat4 proj;
    mat4 proj_inv;
} camera;

// The same buffer the compute shader writes to. There is no vertex input, the particle is looked up by vertex index.
layout(std430, set = 1, binding = 0) readonly buffer Particles {
    Particle particles[];
};

layout(push_constant) uniform PushConstant {
    float pointSize;
    float speedScale;
} pushConstant;

layout(location = 0) out vec3 color;

void main() {
    Particle particle = particles[gl_VertexIndex];

    gl_Position = camera.proj * camera.view * vec4(particle.position.xyz, 1.0);
    gl_PointSize = pushConstant.pointSize;

    // Slow particles are cold, fast particles are hot
    float speed = clamp(length(particle.velocity.xyz) * pushConstant.speedScale, 0.0, 1.0);
    color = mix(vec3(0.1, 0.35, 1.0), vec3(1.0, 0.75, 0.2), speed);
}
