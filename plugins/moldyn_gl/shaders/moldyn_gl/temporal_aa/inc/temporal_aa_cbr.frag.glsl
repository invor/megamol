uniform sampler2D curColorTex;
uniform sampler2D motionVecTex;
uniform sampler2D prevMotionVecTex;
uniform sampler2D depthTex;

layout(binding=0,rgba8)uniform image2D prevColorRead;
layout(binding=1,rgba8)uniform image2D prevColorWrite;

uniform ivec2 resolution;
uniform ivec2 lowResResolution;
uniform vec2 prevJitter;
uniform vec2 curJitter;
uniform int samplingSequencePosition;
uniform mat4 lastViewProjMx;
uniform mat4 invViewMx;
uniform mat4 invProjMx;

in vec2 uvCoords;

out vec4 fragOut;

vec3 depthToWorldPos(float depth,vec2 uv){
    float z=depth*2.-1.;
    
    vec4 cs_pos=vec4(uv*2.-1.,z,1.);
    vec4 vs_pos=invProjMx*cs_pos;
    
    // Perspective division
    vs_pos/=vs_pos.w;
    
    vec4 ws_pos=invViewMx*vs_pos;
    
    return ws_pos.xyz;
}

void main(){
    const ivec2 imgCoord=ivec2(int(uvCoords.x*float(resolution.x)),int(uvCoords.y*float(resolution.y)));
    const ivec2 lowResImgCoord=ivec2(int(uvCoords.x*float(lowResResolution.x)),int(uvCoords.y*float(lowResResolution.y)));
    
    // get reprojected position for previous color texture
    float depth=texelFetch(depthTex,lowResImgCoord,0).r;
    vec3 worldPos=depthToWorldPos(depth,uvCoords);
    vec3 vel=texelFetch(motionVecTex,lowResImgCoord,0).rgb;
    vec3 prevWorldPos=worldPos-vel;
    
    vec4 clipCoord=lastViewProjMx*vec4(prevWorldPos,1);
    clipCoord=clipCoord/clipCoord.w;
    clipCoord=(clipCoord+1.)/2.;
    ivec2 reprojectedImgCoords=ivec2(int(clipCoord.x*float(resolution.x)),int(clipCoord.y*float(resolution.y)));
    
    vec4 curColor=vec4(0.f);
    
    // Checkerboard rendering resolve
    if((samplingSequencePosition==0&&imgCoord.x%2==0)||(samplingSequencePosition==1&&imgCoord.x%2==1)){
        curColor=texelFetch(curColorTex,lowResImgCoord,0);
    }else{
        curColor=imageLoad(prevColorRead,reprojectedImgCoords);
    }
    
    #ifdef TAA
    // Arbitrary out of range numbers
    vec4 minColor=vec4(9999.f,9999.f,9999.f,1.f);
    vec4 maxColor=vec4(-9999.f,-9999.f,-9999.f,1.f);
    
    // Sample a 2x2 neighborhood to create a box in color space
    // compared to normal TAA just 2x2 because of lower resolution (this produces smoother edges compared too using 3x3)
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
    
    // velocity rejection
    ivec2 lowresReprojectedCoords=ivec2(int(clipCoord.x*float(lowResResolution.x)),int(clipCoord.y*float(lowResResolution.y)));
    vec3 prevVel=texelFetch(motionVecTex,lowresReprojectedCoords,0).rgb;
    float velLength=length(prevVel-vel);
    float velDisocclusion=clamp((velLength-.001)*100,0.,1.);
    vec4 curColorClamped=clamp(curColor,minColor,maxColor);
    
    // TAA resolve
    vec4 prevColor=imageLoad(prevColorRead,reprojectedImgCoords);
    
    // Clamp previous color to min/max bounding box
    vec4 previousColorClamped=clamp(prevColor,minColor,maxColor);
    
    vec4 color=mix(.1*curColor+.9*previousColorClamped,curColorClamped,velDisocclusion);
    
    imageStore(prevColorWrite,imgCoord,color);
    fragOut=color;
    #else
    imageStore(prevColorWrite,imgCoord,curColor);
    fragOut=curColor;
    #endif
}