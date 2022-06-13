#version 450
#include "common/common.inc.glsl"
out vec2 uvCoords;
layout (binding=7, r32ui) uniform uimage2D imgRead;

void main() {
    int left = (gl_InstanceID / 600);
    int right = (gl_InstanceID % 600);

    vec2 coord = vec2(float(gl_VertexID % 2), float((gl_VertexID % 4) / 2));

    //gl_Position = vec4(2.0f * coord - 1.0f, 0.0f, 1.0f);
    if(imageLoad(imgRead, ivec2(left, right)).x > 0u){
        //gl_Position = vec4(2.0f * coord - 1.0f, 0.0f, 1.0f);
        gl_Position = vec4(0.2 * gl_VertexID  - 0.5, 1 * (gl_VertexID * (right/600.0) + ((gl_VertexID + 1) % 2) * (left/600.0)) -0.3, 0.0f, 1.0f);
    }else{
        gl_Position = vec4(0);
    }
    
    uvCoords = coord;
}
