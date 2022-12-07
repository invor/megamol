#version 450

#if defined(AMORTIZATION)
#include "moldyn_gl/temporal_aa/inc/temporal_aa_amortization.frag.glsl"
#elif defined(CBR)
#include "moldyn_gl/temporal_aa/inc/temporal_aa_cbr.frag.glsl"
#elif defined(NONE)
#include "moldyn_gl/temporal_aa/inc/temporal_aa_native.frag.glsl"
#endif