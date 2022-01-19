#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 viexw;
    mat4 proj;
} ubo;

layout(push_constant) uniform PushConsts {
	layout(offset = 16) vec4 color;
	vec4 position;
} pushConsts;

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
