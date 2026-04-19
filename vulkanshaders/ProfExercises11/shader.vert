#version 450

layout(location = 0) in vec3 position;

layout(location = 0) out vec3 outPos;

layout(push_constant) uniform PushConstants {
    mat4 modelMat;
} pc;

void main() {
    vec4 pos = vec4(position, 1.0);
    pos = pc.modelMat * pos;

    outPos = vec3(pos);
    gl_Position = pos;
}