#version 450

#if defined(CBR)
#include "moldyn_gl/temporal_aa/inc/temporal_aa_cbr.frag.glsl"
#elif defined(NATIVE)
#include "moldyn_gl/temporal_aa/inc/temporal_aa_native.frag.glsl"
#else
#include "moldyn_gl/temporal_aa/inc/temporal_aa_none.frag.glsl"
#endif