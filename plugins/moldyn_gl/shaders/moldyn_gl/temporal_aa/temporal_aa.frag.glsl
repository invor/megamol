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
    
    // Arbitrary out of range numbers
    vec4 min_color=vec4(9999.f,9999.f,9999.f,1.f);
    vec4 max_color=vec4(-9999.f,-9999.f,-9999.f,1.f);
    
    // Sample a 3x3 neighborhood to create a box in color space
    for(int x=-1;x<=1;++x)
    {
        for(int y=-1;y<=1;++y)
        {
            ivec2 cur_coord=imgCoord+ivec2(x,y);
            vec4 temp_color=texelFetch(curr_color_tex,cur_coord,0);;// Sample neighbor
            min_color=min(min_color,temp_color);// Take min and max
            max_color=max(max_color,temp_color);
        }
    }
    
    prev_color=texelFetch(prev_color_tex,imgCoord,0);
    // Clamp previous color to min/max bounding box
    vec4 previousColorClamped=clamp(prev_color,min_color,max_color);
    
    cur_color=texelFetch(curr_color_tex,imgCoord,0);
    float alpha=cur_color.a;
    color=.1*cur_color+.9*previousColorClamped;
    color.a=1.f;
    
    imageStore(imgPosWrite,imgCoord,vec4(posWorldSpace,0.f,0.f));
    
    imageStore(imgWrite,imgCoord,color);
    fragOut=color;
}