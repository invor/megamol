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

    this->halton_scale_ = 100.0f; // TODO: make this variable changeable in megamol UI
    this->num_samples_ = 4;       // TODO: make this variable changeable in megamol UI
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

    // pass through variables
    rhs_chained_call->SetTime(call.Time());
    rhs_chained_call->SetInstanceTime(call.InstanceTime());
    rhs_chained_call->SetLastFrameTime(call.LastFrameTime());
    rhs_chained_call->SetBackgroundColor(call.BackgroundColor());
    rhs_chained_call->AccessBoundingBoxes() = call.GetBoundingBoxes();
    rhs_chained_call->SetViewResolution(call.GetViewResolution());
    auto const& lhs_input_fbo = call.GetFramebuffer();
    auto const& cam = call.GetCamera();
    auto const& bg = call.BackgroundColor();

    this->projection_ = cam.getProjectionMatrix();
    this->view_ = cam.getViewMatrix();
    this->resolution_ = glm::vec2(lhs_input_fbo->getWidth(), lhs_input_fbo->getHeight());
    this->total_frames_++;

    // Compared to Amortization we just pass the previous frame buffer
    //fbo_->bind();
    //glClearColor(bg.r * bg.a, bg.g * bg.a, bg.b * bg.a, bg.a);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // now jitter the camera
    auto jittered_cam = cam;
    setupCamera(jittered_cam);

    rhs_chained_call->SetFramebuffer(call.GetFramebuffer());
    rhs_chained_call->SetCamera(jittered_cam);
    (*rhs_chained_call)(core::view::AbstractCallRender::FnRender);
    /*
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
    */
    return true;
}

void TemporalAA::setupCamera(core::view::Camera& cam) {
    float delta_width = 1. / this->resolution_.x;
    float delta_height = 1. / this->resolution_.y;

    glm::uint index = this->total_frames_ % this->num_samples_;

    // same formula as in unreal talk
    glm::vec2 jitter = glm::vec2((this->halton_sequence_[index].x * 2.0f - 1.0f) * delta_width,
        (this->halton_sequence_[index].y * 2.0f - 1.0f) * delta_height);

    // again compared to Amortization we do nothing with the intrinsics
    //auto intrinsics = cam.get<core::view::Camera::PerspectiveParameters>();
    auto pose = cam.get<core::view::Camera::Pose>();

    // TODO: Some different implementations additonaly multiply the jitter by a factor (halton_scale_)
    pose.position += glm::vec3(jitter.x * this->halton_scale_, jitter.y * this->halton_scale_, 0.0f);

    //cam.setOrthographicProjection(intrinsics);
    cam.setPose(pose);
}
