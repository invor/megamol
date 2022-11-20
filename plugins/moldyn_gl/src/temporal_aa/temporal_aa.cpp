#include "temporal_aa.h"

#include "compositing_gl/CompositingCalls.h"
#include "halton_sequence.h"
#include "mmcore/CoreInstance.h"

using namespace megamol::moldyn_gl;
using namespace megamol::compositing_gl;

TemporalAA::TemporalAA(void)
        : core::view::RendererModule<CallRender3DGL, mmstd_gl::ModuleGL>()
        , m_motion_vector_texture_call("Motion Vectors Texture", "Access motion vector texture texture") {
    this->m_motion_vector_texture_call.SetCompatibleCall<CallTexture2DDescription>();
    this->MakeSlotAvailable(&this->m_motion_vector_texture_call);
    this->MakeSlotAvailable(&this->chainRenderSlot);
    this->MakeSlotAvailable(&this->renderSlot);
}

TemporalAA::~TemporalAA(void) {
    this->Release();
}

bool TemporalAA::create() {
    auto const shader_options = msf::ShaderFactoryOptionsOpenGL(this->GetCoreInstance()->GetShaderPaths());

    try {
        this->temporal_aa_prgm_ = core::utility::make_glowl_shader("temporal_aa", shader_options,
            "moldyn_gl/temporal_aa/temporal_aa.vert.glsl", "moldyn_gl/temporal_aa/temporal_aa.frag.glsl");
    } catch (std::exception& e) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(("TemporalAA: " + std::string(e.what())).c_str());
        return false;
    }

    this->fbo_ = std::make_shared<glowl::FramebufferObject>(1, 1);
    this->fbo_->createColorAttachment(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    this->fbo_->createColorAttachment(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

    // create halton numbers
    for (int iter = 0; iter < 128; iter++) {
        this->halton_sequence_[iter] =
            glm::vec2(halton::createHaltonNumber(iter + 1, 2), halton::createHaltonNumber(iter + 1, 3));
    }

    this->halton_scale_ = 1.0f; // TODO: make this variable changeable in megamol UI
    this->num_samples_ = 4;     // TODO: make this variable changeable in megamol UI
    this->total_frames_ = 0;

    // Store texture layout for later resize
    this->texLayout_ = glowl::TextureLayout(GL_RGBA8, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1,
        {
            {GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_MIN_FILTER, GL_NEAREST},
            {GL_TEXTURE_MAG_FILTER, GL_NEAREST},
        },
        {});
    this->distTexLayout_ = glowl::TextureLayout(GL_RG32F, 1, 1, 1, GL_RG, GL_FLOAT, 1,
        {
            {GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_MIN_FILTER, GL_NEAREST},
            {GL_TEXTURE_MAG_FILTER, GL_NEAREST},
        },
        {});

    this->texRead_ = std::make_unique<glowl::Texture2D>("texStoreA", texLayout_, nullptr);
    this->texWrite_ = std::make_unique<glowl::Texture2D>("texStoreB", texLayout_, nullptr);
    this->distTexRead_ = std::make_unique<glowl::Texture2D>("distTexR", distTexLayout_, nullptr);
    this->distTexWrite_ = std::make_unique<glowl::Texture2D>("distTexW", distTexLayout_, nullptr);
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

    int const w = lhs_input_fbo->getWidth();
    int const h = lhs_input_fbo->getHeight();

    if (oldWidth_ != w || oldHeight_ != h) {
        oldWidth_ = w;
        oldHeight_ = h;
        setupTextures();
    }

    // pass through variables
    rhs_chained_call->SetTime(call.Time());
    rhs_chained_call->SetInstanceTime(call.InstanceTime());
    rhs_chained_call->SetLastFrameTime(call.LastFrameTime());
    rhs_chained_call->SetBackgroundColor(call.BackgroundColor());
    rhs_chained_call->AccessBoundingBoxes() = call.GetBoundingBoxes();
    rhs_chained_call->SetViewResolution(call.GetViewResolution());

    lastViewProjMx_ = viewProjMx_;
    viewProjMx_ = cam.getProjectionMatrix() * cam.getViewMatrix();

    this->view_ = cam.getViewMatrix();
    this->resolution_ = glm::vec2(lhs_input_fbo->getWidth(), lhs_input_fbo->getHeight());
    this->total_frames_++;

    //glActiveTexture(GL_TEXTURE10);
    //lhs_input_fbo->bindColorbuffer(10);

    // Compared to Amortization we just pass the previous frame buffer
    //this->fbo_ = lhs_input_fbo;
    //this->fbo_->bind();
    //glClearColor(bg.r * bg.a, bg.g * bg.a, bg.b * bg.a, bg.a);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // now jitter the camera
    auto jittered_cam = cam;
    setupCamera(jittered_cam);


    rhs_chained_call->SetFramebuffer(lhs_input_fbo);
    rhs_chained_call->SetCamera(jittered_cam);

    (*rhs_chained_call)(core::view::AbstractCallRender::FnRender);

    // in first frame just use the same colorbuffer
    if (this->total_frames_ == 1) {
        lhs_input_fbo->getColorAttachment(0)->copy(
            lhs_input_fbo->getColorAttachment(0).get(), fbo_->getColorAttachment(1).get());
    }

    glViewport(0, 0, w, h);
    lhs_input_fbo->bind();

    const glm::dmat4 shiftMx = glm::dmat4(lastViewProjMx_) * glm::inverse(glm::dmat4(viewProjMx_));

    const auto intrinsics = cam.get<core::view::Camera::PerspectiveParameters>();

    float frustrum_height = glm::tan(intrinsics.fovy * 0.5f) * 2.0f;
    temporal_aa_prgm_->use();

    temporal_aa_prgm_->setUniform("resolution", w, h);
    temporal_aa_prgm_->setUniform("shiftMx", glm::mat4(shiftMx));
    temporal_aa_prgm_->setUniform("camCenter", cam.getPose().position);
    temporal_aa_prgm_->setUniform("camAspect", intrinsics.aspect.value());
    temporal_aa_prgm_->setUniform("frustumHeight", frustrum_height);

    glActiveTexture(GL_TEXTURE0);
    lhs_input_fbo->bindColorbuffer(0);
    temporal_aa_prgm_->setUniform("curr_color_tex", 0);

    glActiveTexture(GL_TEXTURE1);
    fbo_->bindColorbuffer(1);
    temporal_aa_prgm_->setUniform("prev_color_tex", 1);

    texRead_->bindImage(0, GL_READ_ONLY);
    texWrite_->bindImage(1, GL_WRITE_ONLY);
    distTexRead_->bindImage(2, GL_READ_ONLY);
    distTexWrite_->bindImage(3, GL_WRITE_ONLY);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glUseProgram(0);

    lhs_input_fbo->getColorAttachment(0)->copy(
        lhs_input_fbo->getColorAttachment(0).get(), fbo_->getColorAttachment(1).get());
    texRead_.swap(texWrite_);
    distTexRead_.swap(distTexWrite_);

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

void TemporalAA::setupTextures() {
    this->fbo_->resize(oldWidth_, oldHeight_);
    this->texLayout_.width = oldWidth_;
    this->texLayout_.height = oldHeight_;
    const std::vector<uint32_t> zeroData(oldWidth_ * oldHeight_, 0); // uin32_t <=> RGBA8.
    this->texRead_ = std::make_unique<glowl::Texture2D>("texRead", texLayout_, zeroData.data());
    this->texWrite_ = std::make_unique<glowl::Texture2D>("texWrite", texLayout_, zeroData.data());

    this->distTexLayout_.width = oldWidth_;
    this->distTexLayout_.height = oldHeight_;
    const std::vector<float> posInit(2 * oldWidth_ * oldHeight_, std::numeric_limits<float>::lowest()); // RG32F
    this->distTexRead_ = std::make_unique<glowl::Texture2D>("distTexRead", distTexLayout_, posInit.data());
    this->distTexWrite_ = std::make_unique<glowl::Texture2D>("distTexWrite", distTexLayout_, posInit.data());
}