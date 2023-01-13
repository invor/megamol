#include "commondefines.glsl"
#include "mmstd_gl/flags/bitflags.inc.glsl"
#include "mmstd_gl/common/tflookup.inc.glsl"
#include "mmstd_gl/common/tfconvenience.inc.glsl"

#include "vertex_function.inc.glsl"

/////////////////////////////////////////////////////////////////
//#include "moldyn_gl/sphere_renderer/inc/flags_snippet.inc.glsl"
#ifdef flags_available
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_gpu_shader_fp64 : enable
#endif

/////////////////////////////////////////////////////////////////////
//#include "moldyn_gl/sphere_renderer/inc/vertex_attributes.inc.glsl"
out vec4 objPos;
out vec4 camPos;
out vec4 outlightDir;
out float squarRad;
out float rad;
out vec4 vertColor;
out vec3 velocity;

#ifdef DEFERRED_SHADING
out float pointSize;
#endif // DEFERRED_SHADING

// OUTLINE / ifdef RETICLE
out float sphere_frag_radius;
out vec2 sphere_frag_center;
// OUTLINE
uniform float outlineWidth = 0.0;

uniform vec4 viewAttr;
uniform vec4 lightDir;

#ifdef WITH_SCALING
uniform float scaling;
#endif // WITH_SCALING

#ifndef CALC_CAM_SYS
uniform vec3 camIn;
uniform vec3 camUp;
uniform vec3 camRight;
#endif // CALC_CAM_SYS

uniform vec4 clipDat;
uniform vec4 clipCol;

uniform mat4 MVinv;
uniform mat4 MVtransp;
uniform mat4 MVP;
uniform mat4 MVPinv;
uniform mat4 MVPtransp;

uniform float constRad;
uniform vec4 globalCol;
uniform int useGlobalCol;
uniform int useTf;


//////////////////////////////////////////////////////////////////////////////////
//#include "moldyn_gl/sphere_renderer/inc/sphere_flags_vertex_attributes.inc.glsl"
#define SSBO_FLAGS_BINDING_POINT 2

uniform uint flags_enabled;
uniform vec4 flag_selected_col;
uniform vec4 flag_softselected_col;
uniform uint flags_offset;

#ifdef FLAGS_AVAILABLE
    layout(std430, binding = SSBO_FLAGS_BINDING_POINT) buffer flags {
        uint inFlags[];
    };
#endif // FLAGS_AVAILABLE


//////////////////////////////////////////////////////////////////
//#include "moldyn_gl/sphere_renderer/inc/vertex_mainstart.inc.glsl"
layout (location = 0) in vec4 inPosition;
layout (location = 1) in vec4 inColor;
layout (location = 2) in float inColIdx;
layout (location = 3) in vec3 inVelocity;

void main(void) {

    velocity = inVelocity;
    
    // Remove the sphere radius from the w coordinates to the rad varyings
    vec4 inPos = inPosition;
    rad = (constRad < -0.5) ? inPos.w : constRad;
    inPos.w = 1.0;
        
#ifdef WITH_SCALING
    rad *= scaling;
#endif // WITH_SCALING
    
    squarRad = rad * rad;

#ifdef HALO
    squarRad = (rad + HALO_RAD) * (rad + HALO_RAD);
#endif // HALO

/////////////////////////////////////////////////////////////////////////////
//#include "moldyn_gl/sphere_renderer/inc/sphere_flags_vertex_getflag.inc.glsl"
    uint flag = getFlag(flags_offset + uint(gl_VertexID), bool(flags_enabled));


//////////////////////////////////////////////////////////////
//#include "moldyn_gl/sphere_renderer/inc/vertex_color.inc.glsl"
    // Color  
    vertColor = inColor;
    if (bool(useGlobalCol))  {
        vertColor = globalCol;
    }
    if (bool(useTf)) {
        vertColor = tflookup(inColIdx);
    }
    // Overwrite color depending on flags
    if (bool(flags_enabled)) {
        if (bitflag_test(flag, FLAG_SELECTED, FLAG_SELECTED)) {
            vertColor = flag_selected_col;
        }
        if (bitflag_test(flag, FLAG_SOFTSELECTED, FLAG_SOFTSELECTED)) {
            vertColor = flag_softselected_col;
        }
        //if (!bitflag_isVisible(flag)) {
        //    vertColor = vec4(0.0, 0.0, 0.0, 0.0);
        //}
    }


/////////////////////////////////////////////////////////////////
//#include "moldyn_gl/sphere_renderer/inc/vertex_postrans.inc.glsl"
    // Position transformations
    
    // object pivot point in object space     
    objPos = inPos; // no w-div needed, because w is 1.0 (Because I know) 
 
    // calculate cam position 
    camPos = MVinv[3]; // (C) by Christoph 
    camPos.xyz -= objPos.xyz; // cam pos to glyph space 
 
    // calculate light direction in glyph space 
    outlightDir = lightDir;


/////////////////////////////////////////////////////////////////////////
//#include "moldyn_gl/sphere_renderer/inc/vertex_spheretouchplane.inc.glsl"
    // Sphere-Touch-Plane-Approach
    
    vec2 winHalf = 2.0 / viewAttr.zw; // window size

    vec2 d, p, q, h, dd;

    // get camera orthonormal coordinate system
    vec4 tmp;

#ifdef CALC_CAM_SYS
    // camera coordinate system in object space
    tmp = MVinv[3] + MVinv[2];
    vec3 camIn = normalize(tmp.xyz);
    tmp = MVinv[3] + MVinv[1];
    vec3 camUp = tmp.xyz;
    vec3 camRight = normalize(cross(camIn, camUp));
    camUp = cross(camIn, camRight);
#endif // CALC_CAM_SYS

    vec2 mins, maxs;
    vec3 testPos;
    vec4 projPos;

    // projected camera vector
    vec3 c2 = vec3(dot(camPos.xyz, camRight), dot(camPos.xyz, camUp), dot(camPos.xyz, camIn));

    vec3 cpj1 = camIn * c2.z + camRight * c2.x;
    vec3 cpm1 = camIn * c2.x - camRight * c2.z;

    vec3 cpj2 = camIn * c2.z + camUp * c2.y;
    vec3 cpm2 = camIn * c2.y - camUp * c2.z;
    
    d.x = length(cpj1);
    d.y = length(cpj2);

    dd = vec2(1.0) / d;

    p = squarRad * dd;
    q = d - p;
    h = sqrt(p * q);
    //h = vec2(0.0);
    
    p *= dd;
    h *= dd;

    cpj1 *= p.x;
    cpm1 *= h.x;
    cpj2 *= p.y;
    cpm2 *= h.y;

    // TODO: rewrite only using four projections, additions in homogenous coordinates and delayed perspective divisions.
    testPos = objPos.xyz + cpj1 + cpm1;
    projPos = MVP * vec4(testPos, 1.0);
    projPos /= projPos.w;
    mins = projPos.xy;
    maxs = projPos.xy;

    testPos -= 2.0 * cpm1;
    projPos = MVP * vec4(testPos, 1.0);
    projPos /= projPos.w;
    mins = min(mins, projPos.xy);
    maxs = max(maxs, projPos.xy);

    testPos = objPos.xyz + cpj2 + cpm2;
    projPos = MVP * vec4(testPos, 1.0);
    projPos /= projPos.w;
    mins = min(mins, projPos.xy);
    maxs = max(maxs, projPos.xy);

    testPos -= 2.0 * cpm2;
    projPos = MVP * vec4(testPos, 1.0);
    projPos /= projPos.w;
    mins = min(mins, projPos.xy);
    maxs = max(maxs, projPos.xy);
    
    testPos = objPos.xyz - camIn * rad;
    projPos = MVP * vec4(testPos, 1.0);
    projPos /= projPos.w;


///////////////////////////////////////////////////////////////////
//#include "moldyn_gl/sphere_renderer/inc/vertex_clipping.inc.glsl"
    // Clipping
    float od = clipDat.w - 1.0;
    if (any(notEqual(clipDat.xyz, vec3(0, 0, 0)))) {
        od = dot(objPos.xyz, clipDat.xyz) - rad;
    }

    gl_ClipDistance[0] = 1.0;
    if (od > clipDat.w)  {
        gl_ClipDistance[0] = -1.0;
        vertColor = clipCol;
    }


/////////////////////////////////////////////////////////////////
//#include "moldyn_gl/sphere_renderer/inc/vertex_posout.inc.glsl"
    // Set gl_Position depending on flags (no fragment test required for visibility test)
    if (!(bool(flags_enabled)) || (bool(flags_enabled) && bitflag_isVisible(flag))) {
        
        // Outgoing vertex position and point size
        gl_Position = vec4((mins + maxs) * 0.5, projPos.z, 1.0);
        maxs = (maxs - mins) * 0.5 * winHalf;
        gl_PointSize = max(maxs.x, maxs.y) + 0.5;

    } else {
        gl_ClipDistance[0] = -1.0;
    }


//////////////////////////////////////////////////////////////////
//#include "moldyn_gl/sphere_renderer/inc/vertex_mainend.inc.glsl"
    // Debug
    // gl_PointSize = 32.0;

#ifdef SMALL_SPRITE_LIGHTING
    // for normal crowbaring on very small sprites
    outlightDir.w = (clamp(gl_PointSize, 1.0, 5.0) - 1.0) / 4.0;
#else
    outlightDir.w = 1.0;
#endif // SMALL_SPRITE_LIGHTING

    sphere_frag_radius = gl_PointSize / 2.0;
    gl_PointSize += (2.0 * outlineWidth);
    sphere_frag_center = ((gl_Position.xy / gl_Position.w) - vec2(-1.0, -1.0)) / vec2(viewAttr.z, viewAttr.w);

#ifdef DEFERRED_SHADING
    pointSize = gl_PointSize;
#endif // DEFERRED_SHADING

}

