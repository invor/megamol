/*
 * SphereRenderer.h
 *
 * Copyright (C) 2009 by VISUS (Universitaet Stuttgart)
 * Alle Rechte vorbehalten.
 */

#ifndef MEGAMOLCORE_SPHERERENDERER_H_INCLUDED
#define MEGAMOLCORE_SPHERERENDERER_H_INCLUDED


#include "mmcore/moldyn/MultiParticleDataCall.h"
#include "mmcore/utility/MDAOShaderUtilities.h"
#include "mmcore/utility/MDAOVolumeGenerator.h"

#include "mmcore/Call.h"
#include "mmcore/CallerSlot.h"
#include "mmcore/param/ParamSlot.h"
#include "mmcore/CoreInstance.h"
#include "mmcore/FlagStorage.h"
#include "mmcore/FlagCall.h"
#include "mmcore/view/Renderer3DModule.h"
#include "mmcore/view/CallClipPlane.h"
#include "mmcore/view/CallGetTransferFunction.h"
#include "mmcore/view/CallRender3D.h"
#include "mmcore/param/EnumParam.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/BoolParam.h"
#include "mmcore/param/IntParam.h"
#include "mmcore/param/StringParam.h"
#include "mmcore/param/ButtonParam.h"
#include "mmcore/utility/SSBOStreamer.h"
#include "mmcore/utility/SSBOBufferArray.h"

#include "vislib/types.h"
#include "vislib/assert.h"
#include "vislib/graphics/gl/ShaderSource.h"
#include "vislib/graphics/gl/GLSLShader.h"
#include "vislib/graphics/gl/GLSLGeometryShader.h"
#include "vislib/graphics/gl/IncludeAllGL.h"
#include "vislib/graphics/gl/CameraOpenGL.h"
#include "vislib/graphics/CameraParameters.h"
#include "vislib/math/mathfunctions.h"
#include "vislib/math/ShallowMatrix.h"
#include "vislib/math/Vector.h"
#include "vislib/math/Matrix.h"
#include "vislib/math/Cuboid.h"
#include "vislib/Map.h"

#include <map>
#include <tuple>
#include <utility>
#include <cmath>
#include <cinttypes>
#include <chrono>
#include <sstream>
#include <iterator>

#include <GL/glu.h>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <deque>
#include <fstream>
#include <signal.h>

//#include "TimeMeasure.h"


// Minimum OpenGL version for different render modes
#ifdef GL_VERSION_1_4
#define SPHERE_MIN_OGL_SIMPLE
#define SPHERE_MIN_OGL_SIMPLE_CLUSTERED 
#endif // GL_VERSION_1_4

#ifdef GL_VERSION_3_2
#define SPHERE_MIN_OGL_GEOMETRY_SHADER
#endif // GL_VERSION_3_2

#ifdef GL_VERSION_4_2
#define SPHERE_MIN_OGL_SSBO_STREAM
#endif // GL_VERSION_4_2

#ifdef GL_VERSION_4_4
#define SPHERE_FLAG_STORAGE_AVAILABLE
#endif // GL_VERSION_4_4

#ifdef GL_VERSION_4_5
#define SPHERE_MIN_OGL_BUFFER_ARRAY 
#define SPHERE_MIN_OGL_SPLAT 
#define SPHERE_MIN_OGL_AMBIENT_OCCLUSION
#endif // GL_VERSION_4_5

// Minimum GLSL version for all render modes
#define SPHERE_MIN_GLSL_MAJOR (int(1))
#define SPHERE_MIN_GLSL_MINOR (int(3))


namespace megamol {
namespace core {
namespace moldyn {

    using namespace vislib::graphics::gl;

    /**
     * Renderer for simple sphere glyphs.
     */
    class MEGAMOLCORE_API SphereRenderer : public megamol::core::view::Renderer3DModule {
    public:
       
        /**
         * Answer the name of this module.
         *
         * @return The name of this module.
         */
        static const char *ClassName(void) {
            return "SphereRenderer";
        }

        /**
         * Answer a human readable description of this module.
         *
         * @return A human readable description of this module.
         */
        static const char *Description(void) {
            return "Renderer for sphere glyphs providing different modes using e.g. a bit of bleeding-edge features or a geometry shader.";
        }

        /**
         * Answers whether this module is available on the current system.
         *
         * @return 'true' if the module is available, 'false' otherwise.
         */
        static bool IsAvailable(void) {

#ifdef _WIN32
#if defined(DEBUG) || defined(_DEBUG)
            HDC dc = ::wglGetCurrentDC();
            HGLRC rc = ::wglGetCurrentContext();
            if (dc == nullptr) {
                vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR, 
                    "[SphereRenderer] There is no OpenGL rendering context available.");
            }
            if (rc == nullptr) {
                vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR, 
                    "[SphereRenderer] There is no current OpenGL rendering context available from the calling thread.");
            }
            ASSERT(dc != nullptr);
            ASSERT(rc != nullptr);
#endif // DEBUG || _DEBUG
#endif // _WIN32

            bool retval = true;

            // Minimum requirements for all render modes
            if (!GLSLShader::AreExtensionsAvailable()) {
                vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR, 
                    "[SphereRenderer] No render mode is available. Shader extensions are not available.");
                retval = false;
            }
            // (OpenGL Version and GLSL Version might not correlate, see Mesa 3D on Stampede ...)
            if (ogl_IsVersionGEQ(1, 4) == 0) {
                vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR, 
                    "[SphereRenderer] No render mode available. OpenGL version 1.4 or greater is required.");
                retval = false;
            }
            std::string glslVerStr((char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
            std::size_t found = glslVerStr.find(".");
            int major = -1;
            int minor = -1;
            if (found != std::string::npos) {
                major = std::atoi(glslVerStr.substr(0, 1).c_str());
                minor = std::atoi(glslVerStr.substr(found+1, 1).c_str());
            }
            else {
                vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR,
                    "[SphereRenderer] No valid GL_SHADING_LANGUAGE_VERSION string: %s", glslVerStr.c_str());
            }
            vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_INFO,
                "[SphereRenderer] Found GLSL version %d.%d (%s).", major, minor, glslVerStr.c_str());
            if ((major < (SPHERE_MIN_GLSL_MAJOR)) || (major == (SPHERE_MIN_GLSL_MAJOR) && minor < (SPHERE_MIN_GLSL_MINOR))) {
                vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR, 
                    "[SphereRenderer] No render mode available. OpenGL Shading Language version 1.3 or greater is required.");
                retval = false; 
            }
            if (!isExtAvailable("GL_ARB_explicit_attrib_location")) {
                vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_WARN,
                    "[SphereRenderer] No render mode is available. Extension GL_ARB_explicit_attrib_location is not available.");
                retval = false;
            }
            if (!isExtAvailable("GL_ARB_conservative_depth")) {
                vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_WARN,
                    "[SphereRenderer] No render mode is available. Extension GL_ARB_conservative_depth is not available.");
                retval = false;
            }

#ifdef SPHERE_FLAG_STORAGE_AVAILABLE
            vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_WARN,
                "[SphereRenderer] No flag storage available. OpenGL version 4.3 or greater is required.");
#endif // SPHERE_FLAG_STORAGE_AVAILABLE

            return retval;
        }

        /**
         * The get extents callback. The module should set the members of
         * 'call' to tell the caller the extents of its data (bounding boxes
         * and times).
         *
         * @param call The calling call.
         *
         * @return The return value of the function.
         */
        virtual bool GetExtents(megamol::core::view::CallRender3D& call);

        /** Ctor. */
        SphereRenderer(void);

        /** Dtor. */
        virtual ~SphereRenderer(void);

    protected:

        /**
         * Implementation of 'Create'.
         *
         * @return 'true' on success, 'false' otherwise.
         */
        virtual bool create(void);

        /**
         * Implementation of 'Release'.
         */
        virtual void release(void);

        /**
         * The render callback.
         *
         * @param call The calling call.
         *
         * @return The return value of the function.
         */
        virtual bool Render(megamol::core::view::CallRender3D& call);

    private:

        /*********************************************************************/
        /* VARIABLES                                                         */
        /*********************************************************************/

        enum RenderMode {              
            SIMPLE            = 0,
            SIMPLE_CLUSTERED  = 1,
            GEOMETRY_SHADER   = 2,
            SSBO_STREAM       = 3,
            BUFFER_ARRAY      = 4,
            SPLAT             = 5,
            AMBIENT_OCCLUSION = 6 
        };

        typedef std::map <std::tuple<int, int, bool>, std::shared_ptr<GLSLShader> > shaderMap;

        struct gpuParticleDataType {
            GLuint vertexVBO, colorVBO, vertexArray;
        };

        struct gBufferDataType {
            GLuint color, depth, normals;
            GLuint fbo;
        };

        // Current Render State -----------------------------------------------

        float curViewAttrib[4];
        float curClipDat[4];
        float oldClipDat[4];
        float curClipCol[4];
        float curLightPos[4];
        int   curVpWidth;
        int   curVpHeight;
        int   lastVpWidth;
        int   lastVpHeight;
        vislib::math::Matrix<GLfloat, 4, vislib::math::COLUMN_MAJOR> curMVinv;
        vislib::math::Matrix<GLfloat, 4, vislib::math::COLUMN_MAJOR> curMVP;
        vislib::math::Matrix<GLfloat, 4, vislib::math::COLUMN_MAJOR> curMVPinv;
        vislib::math::Matrix<GLfloat, 4, vislib::math::COLUMN_MAJOR> curMVPtransp;

        // --------------------------------------------------------------------

        RenderMode                               renderMode;
        unsigned int                             greyTF;
        bool                                     triggerRebuildGBuffer;

        GLSLShader                               sphereShader;
        GLSLGeometryShader                       sphereGeometryShader;
        GLSLShader                               lightingShader;

        vislib::SmartPtr<ShaderSource>           vertShader;
        vislib::SmartPtr<ShaderSource>           fragShader;
        vislib::SmartPtr<ShaderSource>           geoShader;

        GLuint                                   vertArray;
        SimpleSphericalParticles::ColourDataType colType;
        SimpleSphericalParticles::VertexDataType vertType;
        std::shared_ptr<GLSLShader>              newShader;
        shaderMap                                theShaders;

        GLuint                                   theSingleBuffer;
        unsigned int                             currBuf;
        GLsizeiptr                               bufSize;
        int                                      numBuffers;
        void                                    *theSingleMappedMem;

        std::vector<gpuParticleDataType>         gpuData;
        gBufferDataType                          gBuffer;
        SIZE_T                                   oldHash;
        unsigned int                             oldFrameID;
        bool                                     stateInvalid;
        vislib::math::Vector<float, 2>           ambConeConstants;
        core::utility::MDAOVolumeGenerator      *volGen;

#ifdef SPHERE_FLAG_STORAGE_AVAILABLE
        // Flag Storage
        FlagStorage::FlagVersionType             currentFlagsVersion;
        GLuint                                   flagsBuffer;
#endif // SPHERE_FLAG_STORAGE_AVAILABLE

#if defined(SPHERE_MIN_OGL_BUFFER_ARRAY) || defined(SPHERE_MIN_OGL_SPLAT)
        GLuint                                   singleBufferCreationBits;
        GLuint                                   singleBufferMappingBits;
        std::vector<GLsync>                      fences;
#endif // defined(SPHERE_MIN_OGL_BUFFER_ARRAY) || defined(SPHERE_MIN_OGL_SPLAT)

#ifdef SPHERE_MIN_OGL_SSBO_STREAM
        megamol::core::utility::SSBOStreamer     streamer;
        megamol::core::utility::SSBOStreamer     colStreamer;
        megamol::core::utility::SSBOBufferArray  bufArray;
        megamol::core::utility::SSBOBufferArray  colBufArray;
#endif // SPHERE_MIN_OGL_SSBO_STREAM

        //TimeMeasure                            timer;

        /*********************************************************************/
        /* SLOTS                                                             */
        /*********************************************************************/

        megamol::core::CallerSlot getDataSlot;
        megamol::core::CallerSlot getClipPlaneSlot;
        megamol::core::CallerSlot getTFSlot;
        megamol::core::CallerSlot getFlagsSlot;

        /*********************************************************************/
        /* PARAMETERS                                                        */
        /*********************************************************************/

        megamol::core::param::ParamSlot renderModeParam;
        megamol::core::param::ParamSlot radiusScalingParam;
        megamol::core::param::ParamSlot forceTimeSlot;
        megamol::core::param::ParamSlot useLocalBBoxParam;

        // Affects only Splat rendering ---------------------------------------

        core::param::ParamSlot alphaScalingParam;
        core::param::ParamSlot attenuateSubpixelParam;
        core::param::ParamSlot useStaticDataParam;

        // Affects only Ambient Occlusion rendering: --------------------------

        megamol::core::param::ParamSlot enableLightingSlot;
        megamol::core::param::ParamSlot enableAOSlot;
        megamol::core::param::ParamSlot enableGeometryShader;
        megamol::core::param::ParamSlot aoVolSizeSlot;
        megamol::core::param::ParamSlot aoConeApexSlot;
        megamol::core::param::ParamSlot aoOffsetSlot;
        megamol::core::param::ParamSlot aoStrengthSlot;
        megamol::core::param::ParamSlot aoConeLengthSlot;
        megamol::core::param::ParamSlot useHPTexturesSlot;

        /*********************************************************************/
        /* FUNCTIONS                                                         */
        /*********************************************************************/

        /**
         * Return specified render mode as human readable string.
         */
        static std::string getRenderModeString(RenderMode rm);

        /**
         * TODO: Document
         *
         * @param t           ...
         * @param outScaling  ...
         *
         * @return Pointer to MultiParticleDataCall ...
         */
        MultiParticleDataCall *getData(unsigned int t, float& outScaling);

        /**
         * TODO: Document
         *
         * @param clipDat  Points to four floats ...
         * @param clipCol  Points to four floats ....
         */
        void getClipData(float clipDat[4], float clipCol[4]);

        /**
         * Check if specified render mode or all render mode are available.
         */
        static bool isRenderModeAvailable(RenderMode rm, bool silent = false);

        /**
         * Create shaders for given render mode.
         * 
         * @return True if success, false otherwise.
         */
        bool createResources(void);

        /**
         * Reset all OpenGL resources.
         *
         * @return True if success, false otherwise.
         */
        bool resetResources(void);

        /**
         * Render spheres in different render modes.
         *
         * @param cr3d       Pointer to the current calling render call.
         * @param mpdc       Pointer to the current multi particle data call.
         *
         * @return           True if success, false otherwise.
         */
        bool renderSimple(view::CallRender3D* cr3d, MultiParticleDataCall* mpdc);
        bool renderGeometryShader(view::CallRender3D* cr3d, MultiParticleDataCall* mpdc);
        bool renderSSBO(view::CallRender3D* cr3d, MultiParticleDataCall* mpdc);
        bool renderSplat(view::CallRender3D* cr3d, MultiParticleDataCall* mpdc);
        bool renderBufferArray(view::CallRender3D* cr3d, MultiParticleDataCall* mpdc);
        bool renderAmbientOcclusion(view::CallRender3D* cr3d, MultiParticleDataCall* mpdc);

        /**
         * Set pointers to vertex and color buffers and corresponding shader variables.
         *
         * @param parts            ...
         * @param shader           ...
         * @param vertBuf          ...
         * @param vertPtr          ...
         * @param vertAttribLoc    ...
         * @param colBuf           ...
         * @param colPtr           ...
         * @param colAttribLoc     ...
         * @param colIdxAttribLoc  ...
         */
        template <typename T>
        void setPointers(MultiParticleDataCall::Particles &parts, T &shader,
            GLuint vertBuf, const void *vertPtr, GLuint vertAttribLoc,
            GLuint colBuf,  const void *colPtr,  GLuint colAttribLoc, GLuint colIdxAttribLoc);


        /**
         * Enables the transfer function texture.
         *
         * @param out_size  ...
         *
         * @return  ...
         */
        bool enableTransferFunctionTexture(unsigned int& out_size);

        /**
         * Get bytes and stride.
         *
         * @param parts        ...
         * @param colBytes     ...
         * @param vertBytes    ...
         * @param colStride    ...
         * @param vertStride   ...
         * @param interleaved  ...
         */
        void getBytesAndStride(MultiParticleDataCall::Particles &parts, unsigned int &colBytes, unsigned int &vertBytes,
            unsigned int &colStride, unsigned int &vertStride, bool &interleaved);

        /**
         * Make SSBO vertex shader color string.
         *
         * @param parts        ...
         * @param code         ...
         * @param declaration  ...
         * @param interleaved  ...
         *
         */
        bool makeColorString(MultiParticleDataCall::Particles &parts, std::string &code, std::string &declaration, bool interleaved);

        /**
         * Make SSBO vertex shader position string.
         *
         * @param parts        ...
         * @param code         ...
         * @param declaration  ...
         * @param interleaved  ...
         */
        bool makeVertexString(MultiParticleDataCall::Particles &parts, std::string &code, std::string &declaration, bool interleaved);

        /**
         * Make SSBO shaders.
         *
         * @param vert  ...
         * @param frag  ...
         */
        std::shared_ptr<GLSLShader> makeShader(vislib::SmartPtr<ShaderSource> vert, vislib::SmartPtr<ShaderSource> frag);

        /**
         * Generate SSBO shaders.
         *
         * @param parts  ...
         *
         */
        std::shared_ptr<GLSLShader> generateShader(MultiParticleDataCall::Particles &parts);

        /**
         * Update flag storage.
         *
         * @param partsCount The sum of all particles in all particle lists.
         */
        bool getFlagStorage(unsigned int partsCount);

        /**
         * Lock single.
         *
         * @param syncObj  ...
         */
        void lockSingle(GLsync& syncObj);

        /**
         * Wait single.
         *
         * @param syncObj  ...
         */
        void waitSingle(GLsync& syncObj);

        // ONLY used for Ambient Occlusion rendering: -------------------------

        /**
         * Rebuild the ambient occlusion shaders.
         *
         * @return ...  
         */
        bool rebuildShader(void);

        /**
         * Rebuild the ambient occlusion gBuffer.
         * 
         * @return ...  
         */
        bool rebuildGBuffer(void);

        /**
         * Rebuild working data.
         *
         * @param cr3d  ...
         * @param dataCall    ...
         */
        void rebuildWorkingData(megamol::core::view::CallRender3D* cr3d, megamol::core::moldyn::MultiParticleDataCall* dataCall);

        /**
         * Render particles geometry.
         *
         * @param cr3d  ...
         * @param dataCall    ...
         */
        void renderParticlesGeometry(megamol::core::view::CallRender3D* cr3d, megamol::core::moldyn::MultiParticleDataCall* dataCall);

        /**
         * Render deferred pass.
         *
         * @param cr3d  ...
         */
        void renderDeferredPass(megamol::core::view::CallRender3D* cr3d);

        /**
         * Upload data to GPU.
         *
         * @param gpuData    ...
         * @param particles  ...
         */
        void uploadDataToGPU(const gpuParticleDataType &gpuData, megamol::core::moldyn::MultiParticleDataCall::Particles& particles);

        /**
         * Generate direction shader array string.
         *
         * @param directions      ...
         * @param directionsName  ...
         *
         * @return ...  
         */
        std::string generateDirectionShaderArrayString(const std::vector< vislib::math::Vector< float, int(4) > >& directions, const std::string& directionsName);

        /**
         * Generate 3 cone directions.
         *
         * @param directions  ...
         * @param apex        ...
         */
        void generate3ConeDirections(std::vector< vislib::math::Vector< float, int(4) > >& directions, float apex);

    };

} /* end namespace moldyn */
} /* end namespace core */
} /* end namespace megamol */

#endif /* MEGAMOLCORE_SPHERERENDERER_H_INCLUDED */
