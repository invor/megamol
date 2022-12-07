uniform sampler2D curColorTex;
uniform sampler2D prevColorTex;
uniform sampler2D motionVecTex;

layout(binding=0,rgba8)uniform image2D imgRead;
layout(binding=1,rgba8)uniform image2D imgWrite;
layout(binding=2,rg32f)uniform image2D imgPosRead;
layout(binding=3,rg32f)uniform image2D imgPosWrite;

uniform ivec2 resolution;
uniform ivec2 lowResResolution;
uniform mat4 shiftMx;
uniform vec3 camCenter;
uniform float camAspect;
uniform float frustumHeight;
uniform vec2 prevJitter;
uniform vec2 curJitter;
uniform int numSamples;
uniform int frameIdx;

in vec2 uvCoords;

out vec4 fragOut;

ivec2 pixelOffsetsHighLowRes1D(int highResPos,int lowResPos,int currentIdx,int totalSize){
    // Case 1
    if(highResPos==lowResPos){
        return ivec2(0,0);
    }
    
    // Case 1
    int offset=lowResPos-highResPos;
    if(abs(offset)<=numSamples/2){
        return ivec2(offset,0);
    }
    
    if(highResPos<lowResPos){
        // Case 2
        if(currentIdx>0){
            offset=lowResPos-(highResPos+numSamples);
            return ivec2(offset,-1);
        }else{
            return ivec2(offset,0);
        }
    }else{
        // Case 3
        if(currentIdx<totalSize-1){
            offset=lowResPos+numSamples-highResPos;
            return ivec2(offset,1);
        }else{
            return ivec2(offset,0);
        }
    }
}

vec2 readPosition(ivec2 coords){
    if(coords.x>=0&&coords.x<resolution.x&&coords.y>=0&&coords.y<resolution.y){
        return imageLoad(imgPosRead,coords).xy;
    }
    return vec2(-3.40282e+38,-3.40282e+38);
}

void main(){
    const ivec2 imgCoordCache=ivec2(int(uvCoords.x*float(resolution.x)),int(uvCoords.y*float(resolution.y)));
    const ivec2 quadCoordCache=imgCoordCache%numSamples;// Position within the current a*a quad on the high res texture.
    const int idx=(numSamples*quadCoordCache.y+quadCoordCache.x);// Linear version of quadCoord as as frame id.
    
    if(frameIdx==idx){
        #include "moldyn_gl/temporal_aa/inc/temporal_aa_defines.inc.glsl"
        // Current high res pixel matches exactly the low res pixel of the current pass.
        curColor=texelFetch(curColorTex,lowResImgCoord,0);
        
        imageStore(imgPosWrite,imgCoord,vec4(posWorldSpace,0.f,0.f));
        
        #include "moldyn_gl/temporal_aa/inc/temporal_aa_pass.inc.glsl"
    }else{
        #include "moldyn_gl/temporal_aa/inc/temporal_aa_defines.inc.glsl"
        const ivec2 quadCoord=imgCoord%numSamples;
        // TODO: This has to be done with motion vectors, since our objects our moving
        // Find shifted image coords. This is where the current high res position was in the previous frame.
        const vec4 posClipSpace=vec4(2.f*uvCoords-1.f,0.f,1.f);
        const vec4 lastPosClipSpace=shiftMx*posClipSpace;
        const ivec2 lastImgCoord=ivec2((lastPosClipSpace.xy/2.f+.5f)*vec2(resolution));
        
        const vec2 lastPosWorldSpace=readPosition(lastImgCoord);
        
        // Position of the current low res pixel within the a*a quad.
        const ivec2 idxCoord=ivec2(frameIdx%numSamples,frameIdx/numSamples);
        
        const ivec2 offsetsX=pixelOffsetsHighLowRes1D(quadCoord.x,idxCoord.x,lowResImgCoord.x,lowResResolution.x);
        const ivec2 offsetsY=pixelOffsetsHighLowRes1D(quadCoord.y,idxCoord.y,lowResImgCoord.y,lowResResolution.y);
        const ivec2 offsetHighRes=ivec2(offsetsX.x,offsetsY.x);// Component wise offset to nearest sample.
        const ivec2 offsetLowRes=ivec2(offsetsX.y,offsetsY.y);// Tex coord offset for lookup in low res texture.
        
        ivec2 jittLowResImgCoord=lowResImgCoord+offsetLowRes;
        const ivec2 sampleImgCoord=imgCoord+offsetHighRes;
        
        const vec2 sampleUvCoords=(vec2(sampleImgCoord)+.5f)/vec2(resolution);
        const vec2 samplePosWorldSpace=camCenter.xy+frustumSize*(sampleUvCoords-.5f);
        
        const float distOldSample=length(posWorldSpace-lastPosWorldSpace);
        const float distNewSample=length(posWorldSpace-samplePosWorldSpace);
        
        if(distNewSample<=distOldSample){
            curColor=texelFetch(curColorTex,jittLowResImgCoord,0);
            imageStore(imgPosWrite,imgCoord,vec4(samplePosWorldSpace,0.f,0.f));
            #include "moldyn_gl/temporal_aa/inc/temporal_aa_pass.inc.glsl"
        }else{
            curColor=imageLoad(imgRead,lastImgCoord);// If this sample is nearer the coords should be within the bounds.
            imageStore(imgPosWrite,imgCoord,vec4(lastPosWorldSpace,0.f,0.f));
            #include "moldyn_gl/temporal_aa/inc/temporal_aa_pass.inc.glsl"
        }
    }
    
    // TODO: upscale code from Amortization here
    
}