uniform sampler2D curColorTex;
uniform sampler2D prevColorRead;
uniform sampler2D motionVecTex;
uniform sampler2D prevMotionVecTex;
uniform sampler2D depthTex;

uniform ivec2 resolution;
uniform vec2 prevJitter;
uniform vec2 curJitter;
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
    
    vec4 color=vec4(0.f);
    vec4 curColor=vec4(0.f);
    
    // Arbitrary out of range numbers
    vec4 minColor=vec4(9999.f,9999.f,9999.f,1.f);
    vec4 maxColor=vec4(-9999.f,-9999.f,-9999.f,1.f);
    
    curColor=texelFetch(curColorTex,imgCoord,0);
    
    // Sample a 3x3 neighborhood to create a box in color space
    for(int x=-1;x<=1;++x)
    {
        for(int y=-1;y<=1;++y)
        {
            ivec2 curCoord=imgCoord+ivec2(x,y);
            vec4 tempColor=texelFetch(curColorTex,curCoord,0);// Sample neighbor
            minColor=min(minColor,tempColor);// Take min and max
            maxColor=max(maxColor,tempColor);
        }
    }
    
    // get reprojected position for previous color texture
    float depth=texelFetch(depthTex,imgCoord,0).r;
    vec3 worldPos=depthToWorldPos(depth,uvCoords);
    vec3 vel=texelFetch(motionVecTex,imgCoord-ivec2(prevJitter)-ivec2(curJitter),0).rgb;
    vec3 prevWorldPos=worldPos-vel;
    
    vec4 clipCoord=lastViewProjMx*vec4(prevWorldPos,1);
    clipCoord=clipCoord/clipCoord.w;
    clipCoord=(clipCoord+1.)/2.;
    
    // velocity rejection
    vec3 prevVel=texture(motionVecTex,clipCoord.xy).rgb;
    float velLength=length(prevVel-vel);
    float velDisocclusion=clamp((velLength-.001)*100,0.,1.);
    vec4 curColorClamped=clamp(curColor,minColor,maxColor);
    
    vec4 prevColor=texture(prevColorRead,clipCoord.xy);
    
    // Clamp previous color to min/max bounding box
    vec4 previousColorClamped=clamp(prevColor,minColor,maxColor);
    
    color=mix(.1*curColor+.9*previousColorClamped,curColorClamped,velDisocclusion);
    fragOut=color;
}