// Sample a 2x2 neighborhood to create a box in color space
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

// get reprojected position for previous color texture
const vec2 unijitterdUV=uvCoords-prevJitter-curJitter;
const ivec2 unjitteredImgCoord=ivec2(int(unijitterdUV.x*float(lowResResolution.x)),int(unijitterdUV.y*float(lowResResolution.y)));
const vec4 curVelSample=texelFetch(motionVecTex,unjitteredImgCoord,0);
const vec2 curVel=vec2(curVelSample.r,curVelSample.g);
const vec2 reprojectedUV=uvCoords+curVel;
const ivec2 reprojectedImgCoords=ivec2(int(reprojectedUV.x*float(lowResResolution.x)),int(reprojectedUV.y*float(lowResResolution.y)));

vec4 prevColor=vec4(0.f);

prevColor=texelFetch(prevColorTex,reprojectedImgCoords,0);
// Clamp previous color to min/max bounding box
vec4 previousColorClamped=clamp(prevColor,minColor,maxColor);

color=.1*curColor+.9*previousColorClamped;

imageStore(imgWrite,imgCoord,color);
fragOut=color;