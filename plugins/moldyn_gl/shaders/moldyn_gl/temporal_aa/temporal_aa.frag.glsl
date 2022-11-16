#version 450

//uniform sampler2D prev_color_tex;
uniform sampler2D curr_color_tex;
uniform sampler2D prev_color_tex;

layout(binding=0,rgba8)uniform image2D imgRead;
layout(binding=1,rgba8)uniform image2D imgWrite;
layout(binding=2,rg32f)uniform image2D imgPosRead;
layout(binding=3,rg32f)uniform image2D imgPosWrite;

uniform ivec2 resolution;
uniform mat4 shiftMx;
uniform vec3 camCenter;
uniform float camAspect;
uniform float frustumHeight;

in vec2 uvCoords;

out vec4 fragOut;

void main(){
    const vec2 frustumSize=vec2(frustumHeight*camAspect,frustumHeight);
    
    const ivec2 imgCoord=ivec2(int(uvCoords.x*float(resolution.x)),int(uvCoords.y*float(resolution.y)));
    const vec2 posWorldSpace=camCenter.xy+frustumSize*(uvCoords-.5f);
    
    vec4 prev_color=vec4(0.f);
    vec4 cur_color=vec4(0.f);
    vec4 color=vec4(0.f);
    
    prev_color=texelFetch(prev_color_tex,imgCoord,0);
    cur_color=texelFetch(curr_color_tex,imgCoord,0);
    color=.1*cur_color+.9*prev_color;
    
    imageStore(imgPosWrite,imgCoord,vec4(posWorldSpace,0.f,0.f));
    
    imageStore(imgWrite,imgCoord,color);
    fragOut=color;
}