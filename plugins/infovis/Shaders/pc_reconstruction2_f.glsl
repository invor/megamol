uniform sampler2DArray src_tx2Da;

uniform int frametype;
uniform int h;
uniform int w;
uniform int approach;

uniform mat4 mMa;
uniform mat4 mMb;
uniform mat4 mMc;
uniform mat4 mMd;

uniform mat4 mMatrices[4];

in vec2 uv_coord;
//layout(early_fragment_tests) in;
out vec4 frag_out;

void main()
{
	vec4 p = vec4(2 * uv_coord.x - 1, 2 * uv_coord.y - 1, 1, 1);
	frag_out = 0.25 * texture(src_tx2Da, vec3(0.5 * (mMatrices[0] * p).xy + vec2(0.5), 0));
	frag_out += 0.25 * texture(src_tx2Da, vec3(0.5 * (mMatrices[1] * p).xy + vec2(0.5), 1));
	frag_out += 0.25 * texture(src_tx2Da, vec3(0.5 * (mMatrices[2] * p).xy + vec2(0.5), 2));
	frag_out += 0.25 * texture(src_tx2Da, vec3(0.5 * (mMatrices[3] * p).xy + vec2(0.5), 3));

}