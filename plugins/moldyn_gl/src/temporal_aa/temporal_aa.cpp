#include "temporal_aa.h"

#include "compositing_gl/CompositingCalls.h"
#include "halton_sequence.h"

#include "mmcore/param/EnumParam.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/IntParam.h"

using namespace megamol::moldyn_gl;
using namespace megamol::compositing_gl;

TemporalAA::TemporalAA(void)
        : core::view::RendererModule<CallRender3DGL, mmstd_gl::ModuleGL>()
        , m_motion_vector_texture_call_("Motion Vectors Texture", "Access motion vector texture")
        , m_dummy_motion_vector_tx_(nullptr)
        , m_depth_texture_call_("Depth Texture", "Access depth texture")
        , m_dummy_depth_tx_(nullptr)
        , halton_scale_param("HaltonScale", "Scale factor for camera jitter")
        , num_samples_param("NumOfSamples", "Number of samples")
        , scaling_mode_param("scalingMode", "The scaling mode used.") {
    m_motion_vector_texture_call_.SetCompatibleCall<CallTexture2DDescription>();
    MakeSlotAvailable(&m_motion_vector_texture_call_);
    m_depth_texture_call_.SetCompatibleCall<CallTexture2DDescription>();
    MakeSlotAvailable(&m_depth_texture_call_);
    MakeSlotAvailable(&chainRenderSlot);
    MakeSlotAvailable(&renderSlot);

    halton_scale_param << new core::param::FloatParam(1.0, 0.0, 1000.0, 0.5);
    MakeSlotAvailable(&halton_scale_param);

    num_samples_param << new core::param::IntParam(4, 1, 128, 1);
    MakeSlotAvailable(&num_samples_param);

    core::param::EnumParam* rmp = new core::param::EnumParam(scaling_mode_);
    rmp->SetTypePair(ScalingMode::NONE, getScalingModeString(ScalingMode::NONE).c_str());
    rmp->SetTypePair(ScalingMode::CBR_W_TAA, getScalingModeString(ScalingMode::CBR_W_TAA).c_str());
    rmp->SetTypePair(ScalingMode::CBR_WO_TAA, getScalingModeString(ScalingMode::CBR_WO_TAA).c_str());
    scaling_mode_param << rmp;
    MakeSlotAvailable(&scaling_mode_param);
}

TemporalAA::~TemporalAA(void) {
    Release();
}

bool TemporalAA::create() {
    auto const shader_options =
        core::utility::make_path_shader_options(frontend_resources.get<megamol::frontend_resources::RuntimeConfig>());
    shader_options_flags_ = std::make_unique<msf::ShaderFactoryOptionsOpenGL>(shader_options);

    try {
        shader_options_flags_->addDefinition("NONE");
        temporal_aa_prgm_ = core::utility::make_glowl_shader("temporal_aa", *shader_options_flags_,
            "moldyn_gl/temporal_aa/temporal_aa.vert.glsl", "moldyn_gl/temporal_aa/temporal_aa.frag.glsl");
    } catch (std::exception& e) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(("TemporalAA: " + std::string(e.what())).c_str());
        return false;
    }

    fbo_ = std::make_shared<glowl::FramebufferObject>(1, 1);
    fbo_->createColorAttachment(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

    // create halton numbers
    for (int iter = 0; iter < 32; iter++) {
        halton_sequence_[iter] =
            glm::vec2(halton::createHaltonNumber(iter + 1, 2), halton::createHaltonNumber(iter + 1, 3));
    }

    // Store texture layout for later resize
    texLayout_ = glowl::TextureLayout(GL_RGBA8, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 1,
        {
            {GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_MIN_FILTER, GL_NEAREST},
            {GL_TEXTURE_MAG_FILTER, GL_NEAREST},
        },
        {});

    velTexLayout_ = glowl::TextureLayout(GL_RGB32F, 1, 1, 1, GL_RGB, GL_FLOAT, 1,
        {
            {GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_MIN_FILTER, GL_NEAREST},
            {GL_TEXTURE_MAG_FILTER, GL_NEAREST},
        },
        {});

    old_lowres_color_read_ = std::make_unique<glowl::Texture2D>("oldColorR", texLayout_, nullptr);
    old_lowres_color_write_ = std::make_unique<glowl::Texture2D>("oldColorW", texLayout_, nullptr);
    zero_velocity_texture_ = std::make_unique<glowl::Texture2D>("zeroVelocity", velTexLayout_, nullptr);
    previous_vel_texture_ = std::make_unique<glowl::Texture2D>("prevVelocity", velTexLayout_, nullptr);
    return true;
}

void TemporalAA::release() {}

bool TemporalAA::GetExtents(CallRender3DGL& call) {
    CallRender3DGL* chainedCall = chainRenderSlot.template CallAs<CallRender3DGL>();
    if (chainedCall != nullptr) {
        *chainedCall = call;
        bool retVal = (*chainedCall)(core::view::AbstractCallRender::FnGetExtents);
        call = *chainedCall;
    }
    return true;
}

bool TemporalAA::Render(CallRender3DGL& call) {
    CallRender3DGL* rhs_chained_call = chainRenderSlot.template CallAs<CallRender3DGL>();
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

    halton_scale_ = halton_scale_param.Param<core::param::FloatParam>()->Value();
    num_samples_ = num_samples_param.Param<core::param::IntParam>()->Value();
    scaling_mode_ = ScalingMode(scaling_mode_param.Param<core::param::EnumParam>()->Value());

    if (oldWidth_ != w || oldHeight_ != h || old_num_samples_ != num_samples_ || old_scaling_mode_ != scaling_mode_) {
        old_scaling_mode_ = scaling_mode_;
        old_num_samples_ = num_samples_;
        oldWidth_ = w;
        oldHeight_ = h;

        if (!updateParams())
            return false;
    }

    // pass through variables
    rhs_chained_call->SetTime(call.Time());
    rhs_chained_call->SetInstanceTime(call.InstanceTime());
    rhs_chained_call->SetLastFrameTime(call.LastFrameTime());
    rhs_chained_call->SetBackgroundColor(call.BackgroundColor());
    rhs_chained_call->AccessBoundingBoxes() = call.GetBoundingBoxes();
    rhs_chained_call->SetViewResolution(call.GetViewResolution());

    resolution_ = glm::vec2(lhs_input_fbo->getWidth(), lhs_input_fbo->getHeight());

    fbo_->bind();
    glClearColor(bg.r * bg.a, bg.g * bg.a, bg.b * bg.a, bg.a);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // now jitter the camera
    auto jittered_cam = cam;
    setupCamera(jittered_cam);

    lastViewProjMx_ = viewProjMx_;
    viewProjMx_ = cam.getProjectionMatrix() * cam.getViewMatrix();

    rhs_chained_call->SetFramebuffer(fbo_);
    rhs_chained_call->SetCamera(jittered_cam);

    (*rhs_chained_call)(core::view::AbstractCallRender::FnRender);

    // get rhs texture calls
    std::shared_ptr<glowl::Texture2D> depth_texture = m_dummy_depth_tx_;
    CallTexture2D* dvt = m_depth_texture_call_.CallAs<CallTexture2D>();
    if (dvt != NULL) {
        (*dvt)(0);
        depth_texture = dvt->getData();
    }

    if (depth_texture == nullptr) {
        return false;
    }

    std::shared_ptr<glowl::Texture2D> motion_vector_texture = m_dummy_motion_vector_tx_;
    CallTexture2D* mvt = m_motion_vector_texture_call_.CallAs<CallTexture2D>();
    if (mvt != NULL) {
        (*mvt)(0);
        motion_vector_texture = mvt->getData();
    }

    if (motion_vector_texture == nullptr) {
        return false;
    }

    float cur_time = glm::floor(call.Time());

    if (cur_time == time_) {
        motion_vector_texture = zero_velocity_texture_;
    }

    time_ = cur_time;

    glViewport(0, 0, w, h);
    lhs_input_fbo->bind();

    temporal_aa_prgm_->use();

    temporal_aa_prgm_->setUniform("resolution", w, h);
    temporal_aa_prgm_->setUniform("lowResResolution", fbo_->getWidth(), fbo_->getHeight());
    temporal_aa_prgm_->setUniform("prevJitter", prev_jitter_);
    temporal_aa_prgm_->setUniform("curJitter", cur_jitter_);
    temporal_aa_prgm_->setUniform("samplingSequencePosition", samplingSequencePosition_);
    temporal_aa_prgm_->setUniform("lastViewProjMx", lastViewProjMx_);
    temporal_aa_prgm_->setUniform("invViewMx", glm::inverse(cam.getViewMatrix()));
    temporal_aa_prgm_->setUniform("invProjMx", glm::inverse(cam.getProjectionMatrix()));

    glActiveTexture(GL_TEXTURE0);
    fbo_->bindColorbuffer(0);
    temporal_aa_prgm_->setUniform("curColorTex", 0);

    glActiveTexture(GL_TEXTURE1);
    motion_vector_texture->bindTexture();
    temporal_aa_prgm_->setUniform("motionVecTex", 1);

    glActiveTexture(GL_TEXTURE2);
    previous_vel_texture_->bindTexture();
    temporal_aa_prgm_->setUniform("prevMotionVecTex", 2);

    glActiveTexture(GL_TEXTURE3);
    depth_texture->bindTexture();
    temporal_aa_prgm_->setUniform("depthTex", 3);

    old_lowres_color_read_->bindImage(0, GL_READ_ONLY);
    old_lowres_color_write_->bindImage(1, GL_WRITE_ONLY);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glUseProgram(0);

    motion_vector_texture->copy(motion_vector_texture.get(), previous_vel_texture_.get());

    old_lowres_color_read_.swap(old_lowres_color_write_);

    return true;
}

void TemporalAA::setupCamera(core::view::Camera& cam) {
    glm::vec2 jitter;

    if (scaling_mode_ == ScalingMode::CBR_W_TAA || scaling_mode_ == ScalingMode::CBR_WO_TAA) {
        samplingSequencePosition_ = (samplingSequencePosition_ + 1) % (2);
        jitter = glm::vec2(camOffsets_[samplingSequencePosition_].x, 0.f);

    } else {
        samplingSequencePosition_ = (samplingSequencePosition_ + 1) % num_samples_;
        float delta_width = 1. / resolution_.x;
        float delta_height = 1. / resolution_.y;

        // same formula as in unreal talk
        jitter = glm::vec2((halton_sequence_[samplingSequencePosition_].x * 2.0f - 1.0f) * delta_width,
            (halton_sequence_[samplingSequencePosition_].y * 2.0f - 1.0f) * delta_height);
        jitter *= halton_scale_;
    }

    prev_jitter_ = cur_jitter_;
    cur_jitter_ = jitter;

    auto pose = cam.get<core::view::Camera::Pose>();
    pose.position += glm::vec3(jitter.x, jitter.y, 0.0f);
    cam.setPose(pose);
}

bool TemporalAA::updateParams() {
    samplingSequencePosition_ = 0;
    auto const shader_options =
        core::utility::make_path_shader_options(frontend_resources.get<megamol::frontend_resources::RuntimeConfig>());
    shader_options_flags_ = std::make_unique<msf::ShaderFactoryOptionsOpenGL>(shader_options);

    if (scaling_mode_ == ScalingMode::CBR_WO_TAA || scaling_mode_ == ScalingMode::CBR_W_TAA) {
        shader_options_flags_->addDefinition("CBR");
        if (scaling_mode_ == ScalingMode::CBR_W_TAA) {
            shader_options_flags_->addDefinition("TAA");
        }

        fbo_->resize(oldWidth_ / 2, oldHeight_);
        camOffsets_.resize(2);

        for (int j = 0; j < 2; j++) {
            const float x = static_cast<float>(2 * j - 2 + 1) / (2 * static_cast<float>(oldWidth_));
            camOffsets_[j] = glm::vec3(x, 0.0f, 0.0f);
        }

        velTexLayout_.width = oldWidth_ / 2;
        velTexLayout_.height = oldHeight_;
        const std::vector<float> velocity_zero_data(3 * (oldWidth_ / 2) * oldHeight_, 0.0f);
        zero_velocity_texture_ =
            std::make_unique<glowl::Texture2D>("velZeroData", velTexLayout_, velocity_zero_data.data());
        previous_vel_texture_ =
            std::make_unique<glowl::Texture2D>("prevVelocity", velTexLayout_, velocity_zero_data.data());
    } else {
        shader_options_flags_->addDefinition("NONE");
        fbo_->resize(oldWidth_, oldHeight_);

        velTexLayout_.width = oldWidth_;
        velTexLayout_.height = oldHeight_;
        const std::vector<float> velocity_zero_data(3 * oldWidth_ * oldHeight_, 0.0f);
        zero_velocity_texture_ =
            std::make_unique<glowl::Texture2D>("velZeroData", velTexLayout_, velocity_zero_data.data());
        previous_vel_texture_ =
            std::make_unique<glowl::Texture2D>("prevVelocity", velTexLayout_, velocity_zero_data.data());
    }

    texLayout_.width = oldWidth_;
    texLayout_.height = oldHeight_;
    const std::vector<uint32_t> zero_data(oldWidth_ * oldHeight_, 0);
    old_lowres_color_read_ = std::make_unique<glowl::Texture2D>("oldColorR", texLayout_, zero_data.data());
    old_lowres_color_write_ = std::make_unique<glowl::Texture2D>("oldColorW", texLayout_, zero_data.data());

    viewProjMx_ = glm::mat4(1.0f);
    lastViewProjMx_ = glm::mat4(1.0f);

    // recompile shader
    try {
        temporal_aa_prgm_ = core::utility::make_glowl_shader("temporal_aa", *shader_options_flags_,
            "moldyn_gl/temporal_aa/temporal_aa.vert.glsl", "moldyn_gl/temporal_aa/temporal_aa.frag.glsl");
    } catch (std::exception& e) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(("TemporalAA: " + std::string(e.what())).c_str());
        return false;
    }
    return true;
}

std::string TemporalAA::getScalingModeString(ScalingMode sm) {

    std::string mode;

    switch (sm) {
    case (ScalingMode::NONE):
        mode = "None";
        break;
    case (ScalingMode::CBR_WO_TAA):
        mode = "Checkerboard-Rendering without TAA";
        break;
    case (ScalingMode::CBR_W_TAA):
        mode = "Checkerboard-Rendering with TAA";
        break;
    default:
        mode = "unknown";
        break;
    }

    return mode;
}
