
uint getFlag(uint flag_idx, bool flags_enabled){
    /////////////////////////////////////////////////////////////////////////////
    //#include "moldyn_gl/sphere_renderer/inc/sphere_flags_vertex_getflag.inc.glsl"
    uint flag = uint(0);
    
    #ifdef FLAGS_AVAILABLE
        if (bool(flags_enabled)) {
            flag = inFlags[flag_idx];
        }
    #endif // FLAGS_AVAILABLE

    return flag;
};

//      vec4 getColor(){
//      //////////////////////////////////////////////////////////////
//      //#include "moldyn_gl/sphere_renderer/inc/vertex_color.inc.glsl"
//          // Color  
//          vec4 retval = inColor;
//          if (bool(useGlobalCol))  {
//              retval = globalCol;
//          }
//          if (bool(useTf)) {
//              retval = tflookup(inColIdx);
//          }
//          // Overwrite color depending on flags
//          if (bool(flags_enabled)) {
//              if (bitflag_test(flag, FLAG_SELECTED, FLAG_SELECTED)) {
//                  retval = flag_selected_col;
//              }
//              if (bitflag_test(flag, FLAG_SOFTSELECTED, FLAG_SOFTSELECTED)) {
//                  retval = flag_softselected_col;
//              }
//              //if (!bitflag_isVisible(flag)) {
//              //    vertColor = vec4(0.0, 0.0, 0.0, 0.0);
//              //}
//          }
//      
//          return retval;
//      };
