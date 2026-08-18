#version 460

layout(location = 0) in vec3 color;

layout(location = 0) out vec4 fragColor;

void main() {
    // Turn the square point sprite into a soft round dot
    vec2 offset = 2.0 * gl_PointCoord - 1.0;
    float radiusSquared = dot(offset, offset);

    if (radiusSquared > 1.0) {
        discard;
    }

    // The particles are blended additively, so the falloff makes the dense regions glow
    float falloff = 1.0 - radiusSquared;
    fragColor = vec4(color, falloff);
}
