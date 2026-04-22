#version 450

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec4 interColor;
layout(location = 1) in vec4 interPos;
layout(location = 2) in vec3 interNormal;

struct PointLight {
    vec4 pos;
    vec4 color;
};

layout(set = 0, binding = 1) uniform UBOFragment {
    vec4 cameraPos;
    uint lightCnt;
} ubo;

layout(set = 0, binding = 2, std430) readonly buffer SSBOLights {
    PointLight allLights[];
};


void main()
{
    /*
    vec3 attenValue = vec3(0,0,0);
    for(int i = 0; i < ubo.lightCnt; i++) {
        PointLight light = allLights[i];
        vec3 L = vec3(light.pos - interPos);
        float d = length(L);
        float at = 1.0 / (d*d + 1.0);
        attenValue[i] = at;
    }
    out_color = vec4(attenValue, 1.0);
    */

    vec3 N = normalize(interNormal);
    vec3 finalColor = vec3(0,0,0);

    
    for(int i = 0; i < ubo.lightCnt; i++) {
        PointLight light = allLights[i];
        vec3 L = normalize(vec3(light.pos - interPos));
        float diffuse = max(0, dot(N, L));
        vec3 diffuseColor = diffuse*vec3(light.color);
        finalColor += diffuseColor;  
    }

    out_color = vec4(finalColor, 1.0);

}
