// TODO:
#version 450

layout(location=0)in vec4 position;

uniform mat4 projection;
uniform mat4 view;
uniform vec2 resolution;
uniform uint total_frames;

uniform vec2 halton_sequence[128];
uniform float halton_scale;
uniform uint num_samples;

void main()
{
    float delta_width=1./resolution.x;
    float delta_height=1./resolution.y;
    
    uint index=total_frames%num_samples;
    vec2 jitter=vec2(halton_sequence[index].x*delta_width,halton_sequence[index].y*delta_height);
    
    mat4 jitter_mat=mat4(1.);
    if(halton_scale>0)
    {
        jitter_mat[3][0]+=jitter.x*halton_scale;
        jitter_mat[3][1]+=jitter.y*halton_scale;
    }
    
    gl_Position=jitter_mat*projection*view*position;
}