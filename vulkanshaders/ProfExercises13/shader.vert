#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 normal;


layout(location = 0) out vec4 interColor;
layout(location = 1) out vec4 interPos;
layout(location = 2) out vec3 interNormal;


layout(push_constant) uniform PushConstants {
    mat4 modelMat;
} pc;

layout(set = 0, binding = 0) uniform UBOVertex {
    mat4 viewMat;
    mat4 projMat;
} ubo;

void main()
{
    vec4 pos = vec4(position, 1.0);
    vec4 wpos = pc.modelMat*pos;
    interPos = wpos;
    gl_Position = ubo.projMat*ubo.viewMat*wpos;
    interColor = color;

    mat3 normMat = transpose(inverse(mat3(pc.modelMat)));
    interNormal = normMat * normal;

}
