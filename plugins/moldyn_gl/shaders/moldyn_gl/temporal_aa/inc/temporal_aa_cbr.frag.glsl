uniform sampler2D curColorTex;
uniform sampler2D motionVecTex;

layout(binding=0,rgba8)uniform image2D imgRead;
layout(binding=1,rgba8)uniform image2D imgWrite;
layout(binding=2,rg32f)uniform image2D imgPosRead;
layout(binding=3,rg32f)uniform image2D imgPosWrite;
layout(binding=4,rgba8)uniform image2D prevColorRead;
layout(binding=5,rgba8)uniform image2D prevColorWrite;

uniform ivec2 resolution;
uniform ivec2 lowResResolution;
uniform mat4 shiftMx;
uniform vec3 camCenter;
uniform float camAspect;
uniform float frustumHeight;
uniform vec2 prevJitter;
uniform vec2 curJitter;
uniform int samplingSequencePosition;

in vec2 uvCoords;

out vec4 fragOut;

void main(){
    const ivec2 imgCoord=ivec2(int(uvCoords.x*float(resolution.x)),int(uvCoords.y*float(resolution.y)));
    const ivec2 lowResImgCoord=ivec2(int(uvCoords.x*float(lowResResolution.x)),int(uvCoords.y*float(lowResResolution.y)));
    
    // get reprojected position for previous color texture
    const vec2 unijitterdUV=uvCoords-prevJitter-curJitter;
    const ivec2 unjitteredImgCoord=ivec2(int(unijitterdUV.x*float(lowResResolution.x)),int(unijitterdUV.y*float(lowResResolution.y)));
    const vec4 curVelSample=texelFetch(motionVecTex,unjitteredImgCoord,0);
    const vec2 curVel=vec2(curVelSample.r,curVelSample.g);
    const vec2 reprojectedUV=uvCoords+curVel;
    const ivec2 reprojectedImgCoords=ivec2(int(reprojectedUV.x*float(lowResResolution.x)),int(reprojectedUV.y*float(lowResResolution.y)));
    
    const vec2 frustumSize=vec2(frustumHeight*camAspect,frustumHeight);
    const vec2 posWorldSpace=camCenter.xy+frustumSize*(uvCoords-.5f);
    vec4 color=vec4(0.f);
    vec4 curColor=vec4(0.f);
    
    // Arbitrary out of range numbers
    vec4 minColor=vec4(9999.f,9999.f,9999.f,1.f);
    vec4 maxColor=vec4(-9999.f,-9999.f,-9999.f,1.f);
    
    // Checkerboard rendering resolve
    if((samplingSequencePosition==0&&imgCoord.y%2==0)||(samplingSequencePosition==1&&imgCoord.y%2==1)){
        curColor=texelFetch(curColorTex,lowResImgCoord,0);
    }else{
        //TODO: reprojection at top
        curColor=imageLoad(prevColorRead,reprojectedImgCoords);
    }
    
    imageStore(imgPosWrite,imgCoord,vec4(posWorldSpace,0.f,0.f));
    
    #ifdef TAA
    // Sample a 2x2 neighborhood to create a box in color space
    // compared to normal TAA just 2x2 because of lower resolution (this produces smoother edges compared too using 3x3)
    // TODO: Test which works better
    for(int x=-1;x<=0;++x)
    {
        ivec2 curCoord=lowResImgCoord+ivec2(x,x+1);
        vec4 tempColor=texelFetch(curColorTex,curCoord,0);// Sample neighbor
        minColor=min(minColor,tempColor);// Take min and max
        maxColor=max(maxColor,tempColor);
        
        curCoord=lowResImgCoord+ivec2(x+1,x);
        tempColor=texelFetch(curColorTex,curCoord,0);// Sample neighbor
        minColor=min(minColor,tempColor);// Take min and max
        maxColor=max(maxColor,tempColor);
    }
    
    /*
    for(int x=-1;x<=1;++x)
    {
        for(int y=-1;y<=1;++y)
        {
            ivec2 curCoord=lowResImgCoord+ivec2(x,y);
            vec4 tempColor=texelFetch(curColorTex,curCoord,0);// Sample neighbor
            minColor=min(minColor,tempColor);// Take min and max
            maxColor=max(maxColor,tempColor);
        }
    }
    */
    
    // TAA resolve
    
    vec4 prevColor=vec4(0.f);
    
    prevColor=imageLoad(prevColorRead,reprojectedImgCoords);
    
    // Clamp previous color to min/max bounding box
    vec4 previousColorClamped=clamp(prevColor,minColor,maxColor);
    
    color=.1*curColor+.9*previousColorClamped;
    
    imageStore(imgWrite,imgCoord,color);
    imageStore(prevColorWrite,lowResImgCoord,color);
    fragOut=color;
    #else
    imageStore(imgWrite,imgCoord,curColor);
    imageStore(prevColorWrite,lowResImgCoord,curColor);
    fragOut=curColor;
    #endif
}