#include "temporal_aa.h"

#include "halton_sequence.h"
#include "mmstd_gl/renderer/Renderer3DModuleGL.h"

using namespace megamol::moldyn_gl;

TemporalAA::TemporalAA(void) : core::view::RendererModule<CallRender3DGL, mmstd_gl::ModuleGL>() {
    this->MakeSlotAvailable(&this->chainRenderSlot);
    this->MakeSlotAvailable(&this->renderSlot);
}

TemporalAA::~TemporalAA(void) {
    this->Release();
}

bool TemporalAA::create() {
    auto const shader_options = msf::ShaderFactoryOptionsOpenGL(this->GetCoreInstance()->GetShaderPaths());

    try {
        this->temporal_aa_prgm_ = core::utility::make_glowl_shader(
            "temporal_aa", shader_options, "moldyn_gl/temporal_aa/temporal_aa.vert.glsl");
    } catch (std::exception& e) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(("TemporalAA: " + std::string(e.what())).c_str());
        return false;
    }

    this->fbo_ = std::make_shared<glowl::FramebufferObject>(1, 1);

    // create halton numbers
    for (int iter = 0; iter < 128; iter++) {
        this->halton_sequence_[iter] =
            glm::vec2(halton::createHaltonNumber(iter + 1, 2), halton::createHaltonNumber(iter + 1, 3));
    }

    this->halton_scale_ = 100.0f;
    this->num_samples_ = 8;
    this->total_frames_ = 0;
    return true;
}

void TemporalAA::release() {
    this->Release();
}

bool TemporalAA::GetExtents(CallRender3DGL& call) {
    CallRender3DGL* chainedCall = this->chainRenderSlot.template CallAs<CallRender3DGL>();
    if (chainedCall != nullptr) {
        *chainedCall = call;
        bool retVal = (*chainedCall)(core::view::AbstractCallRender::FnGetExtents);
        call = *chainedCall;
    }
    return true;
}

bool TemporalAA::Render(CallRender3DGL& call) {
    CallRender3DGL* rhs_chained_call = this->chainRenderSlot.template CallAs<CallRender3DGL>();
    if (rhs_chained_call == nullptr) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "The TemporalAA-Module does not work without a renderer attached to its right");
        return false;
    }

    auto const& lhs_input_fbo = call.GetFramebuffer();
    auto const& cam = call.GetCamera();
    auto const& bg = call.BackgroundColor();

    this->projection_ = cam.getProjectionMatrix();
    this->view_ = cam.getViewMatrix();
    this->resolution_ = glm::vec2(lhs_input_fbo->getWidth(), lhs_input_fbo->getHeight());
    this->total_frames_++;

    //lhs_input_fbo->bind();

    temporal_aa_prgm_->use();
    glUniformMatrix4fv(
        this->temporal_aa_prgm_->getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(this->projection_));
    glUniformMatrix4fv(this->temporal_aa_prgm_->getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(this->view_));
    glUniform2fv(this->temporal_aa_prgm_->getUniformLocation("resolution"), 1, glm::value_ptr(this->resolution_));
    glUniform1ui(this->temporal_aa_prgm_->getUniformLocation("total_frames"), this->total_frames_);
    glUniform2fv(
        this->temporal_aa_prgm_->getUniformLocation("halton_sequence"), 1, glm::value_ptr(this->halton_sequence_[0]));
    glUniform1fv(this->temporal_aa_prgm_->getUniformLocation("halton_scale"), 1, &this->halton_scale_);
    glUniform1ui(this->temporal_aa_prgm_->getUniformLocation("num_samples"), this->num_samples_);

    // TODO: Hier bin ich nicht sicher was ich machen soll?
    //glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    //lhs_input_fbo->bindToDraw();
    glUseProgram(0);
    return true;
}
