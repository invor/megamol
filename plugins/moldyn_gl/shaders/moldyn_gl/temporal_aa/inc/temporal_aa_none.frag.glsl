uniform sampler2D curColorTex;

uniform ivec2 resolution;

in vec2 uvCoords;

out vec4 fragOut;

void main(){
    const ivec2 imgCoord=ivec2(int(uvCoords.x*float(resolution.x)),int(uvCoords.y*float(resolution.y)));
    vec4 curColor=texelFetch(curColorTex,imgCoord,0);
    fragOut=curColor;
}