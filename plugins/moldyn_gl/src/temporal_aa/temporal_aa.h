#pragma once

#include <glm/glm.hpp>
#include <glowl/BufferObject.hpp>
#include <glowl/Sampler.hpp>
#include <glowl/glowl.h>

#include "mmcore/Call.h"
#include "mmcore/CalleeSlot.h"
#include "mmcore/CallerSlot.h"
#include "mmcore/param/ParamSlot.h"
#include "mmcore_gl/utility/ShaderFactory.h"
#include "mmstd/renderer/RendererModule.h"
#include "mmstd_gl/ModuleGL.h"
#include "mmstd_gl/renderer/CallRender3DGL.h"
#include "mmstd_gl/renderer/Renderer3DModuleGL.h"

using namespace megamol::mmstd_gl;
namespace megamol {
namespace moldyn_gl {

/**
 * @brief
 *
 */
class TemporalAA : public core::view::RendererModule<CallRender3DGL, mmstd_gl::ModuleGL> {
public:
    /**
     * Answer the name of this module.
     *
     * @return The name of this module.
     */
    static const char* ClassName(void) {
        return "TemporalAA";
    }

    /**
     * Answer a human readable description of this module.
     *
     * @return A human readable description of this module.
     */
    static const char* Description(void) {
        return "Temporal Anti-Aliasing";
    }

    static bool IsAvailable(void) {
        return true;
    }

    /** Constructor */
    TemporalAA(void);

    /** Destructor */
    virtual ~TemporalAA(void);

protected:
    virtual bool create();
    virtual void release();
    virtual bool Render(CallRender3DGL& call);
    virtual void setupCamera(core::view::Camera& cam);
    virtual void updateParams();
    virtual bool GetExtents(CallRender3DGL& call);

private:
    core::param::ParamSlot halton_scale_param;
    core::param::ParamSlot num_samples_param;
    core::param::ParamSlot upscaling_param;

    std::shared_ptr<glowl::FramebufferObject> fbo_;
    std::shared_ptr<glowl::FramebufferObject> old_fbo_; // save the previous color
    std::shared_ptr<glowl::GLSLProgram> temporal_aa_prgm_;
    glowl::TextureLayout texLayout_;
    glowl::TextureLayout distTexLayout_;
    std::unique_ptr<glowl::Texture2D> texRead_;
    std::unique_ptr<glowl::Texture2D> texWrite_;
    std::unique_ptr<glowl::Texture2D> distTexRead_;
    std::unique_ptr<glowl::Texture2D> distTexWrite_;

    /** Dummy motion vector texture to use when no texture is connected */
    std::shared_ptr<glowl::Texture2D> m_dummy_motion_vector_tx_;

    // Render State
    glm::mat4 viewProjMx_;
    glm::mat4 lastViewProjMx_;
    glm::mat4 view_;
    glm::vec2 resolution_;
    int oldWidth_ = -1;
    int oldHeight_ = -1;
    glm::uint frames_ = 0;
    glm::vec2 prev_jitter_;
    glm::vec2 cur_jitter_;

    glm::uint
        num_samples_; // either the rotation of the halton_sequence or the upscaling factor (when upscaling turned on)
    glm::uint old_num_samples_;
    bool upscaling_ = false;
    bool old_upscaling_ = false;
    int frameIdx_ = 0;
    int samplingSequencePosition_;

    std::vector<int> samplingSequence_;
    std::vector<glm::vec3> camOffsets_;

    // input slot for motion vectors
    core::CallerSlot m_motion_vector_texture_call_;

    // halton variables
    glm::vec2 halton_sequence_[128];
    float halton_scale_;
};
} // namespace moldyn_gl
} // namespace megamol
