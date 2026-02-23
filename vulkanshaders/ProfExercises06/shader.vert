#version 450

layout(location = 0) in vec3 position;

layout(location = 0) out vec3 outPos;

void main() {

    outPos = position;
    gl_Position = vec4(position, 1.0);
}