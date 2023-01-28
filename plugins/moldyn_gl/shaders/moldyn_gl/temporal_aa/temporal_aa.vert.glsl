#version 450

#include "mmstd_gl/common/quad_vertices.inc.glsl"

out vec2 uvCoords;

void main(){
    vec2 coord=quadVertexPosition();
    
    gl_Position=vec4(2.f*coord-1.f,0.f,1.f);
    uvCoords=coord;
}