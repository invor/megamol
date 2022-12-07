const vec2 frustumSize=vec2(frustumHeight*camAspect,frustumHeight);
const ivec2 imgCoord=ivec2(int(uvCoords.x*float(resolution.x)),int(uvCoords.y*float(resolution.y)));
const ivec2 lowResImgCoord=ivec2(int(uvCoords.x*float(lowResResolution.x)),int(uvCoords.y*float(lowResResolution.y)));
const vec2 posWorldSpace=camCenter.xy+frustumSize*(uvCoords-.5f);

vec4 color=vec4(0.f);
vec4 curColor=vec4(0.f);

// Arbitrary out of range numbers
vec4 minColor=vec4(9999.f,9999.f,9999.f,1.f);
vec4 maxColor=vec4(-9999.f,-9999.f,-9999.f,1.f);