#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec4 interColor;

layout(push_constant) uniform PushConstants {
    mat4 modelMat;
} pc;

layout(set = 0, binding = 0) uniform UBOVertex {
    mat4 viewMat;
    mat4 projMat;
} ubo;

void main() {
    interColor = color;

    vec4 pos = vec4(position, 1.0);
    pos = pc.modelMat * pos;
    outPos = vec3(pos);

    pos = ubo.projMat * ubo.viewMat * pos;
    gl_Position = pos;
}