#version 460

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 fragColor;

//layout(binding = 0) uniform image2D rayTraceImage;

void main() {
//imageLoad(rayTraceImage
//    fragColor = texture(rayTraceImage, inUV);
	fragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
