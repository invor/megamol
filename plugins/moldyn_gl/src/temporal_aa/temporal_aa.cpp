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
        , m_motion_vector_texture_call_("Motion Vectors Texture", "Access motion vector texture texture")
        , m_dummy_motion_vector_tx_(nullptr)
        , halton_scale_param("HaltonScale", "Scale factor for camera jitter")
        , num_samples_param("NumOfSamples", "Number of samples")
        , scaling_mode_param("scalingMode", "The scaling mode used.") {
    m_motion_vector_texture_call_.SetCompatibleCall<CallTexture2DDescription>();
    MakeSlotAvailable(&m_motion_vector_texture_call_);
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
    old_fbo_ = std::make_shared<glowl::FramebufferObject>(1, 1);
    old_fbo_->createColorAttachment(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

    // create halton numbers
    for (int iter = 0; iter < 128; iter++) {
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
    distTexLayout_ = glowl::TextureLayout(GL_RG32F, 1, 1, 1, GL_RG, GL_FLOAT, 1,
        {
            {GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER},
            {GL_TEXTURE_MIN_FILTER, GL_NEAREST},
            {GL_TEXTURE_MAG_FILTER, GL_NEAREST},
        },
        {});

    texRead_ = std::make_unique<glowl::Texture2D>("texStoreA", texLayout_, nullptr);
    texWrite_ = std::make_unique<glowl::Texture2D>("texStoreB", texLayout_, nullptr);
    old_lowres_color_read_ = std::make_unique<glowl::Texture2D>("oldColorR", texLayout_, nullptr);
    old_lowres_color_write_ = std::make_unique<glowl::Texture2D>("oldColorW", texLayout_, nullptr);
    distTexRead_ = std::make_unique<glowl::Texture2D>("distTexR", distTexLayout_, nullptr);
    distTexWrite_ = std::make_unique<glowl::Texture2D>("distTexW", distTexLayout_, nullptr);
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
    frames_++;

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

    lastViewProjMx_ = viewProjMx_;
    viewProjMx_ = cam.getProjectionMatrix() * cam.getViewMatrix();

    view_ = cam.getViewMatrix();
    resolution_ = glm::vec2(lhs_input_fbo->getWidth(), lhs_input_fbo->getHeight());

    fbo_->bind();
    glClearColor(bg.r * bg.a, bg.g * bg.a, bg.b * bg.a, bg.a);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // now jitter the camera
    auto jittered_cam = cam;
    setupCamera(jittered_cam);

    //rhs_chained_call->SetViewResolution({fbo_->getWidth(), fbo_->getHeight()});
    rhs_chained_call->SetFramebuffer(fbo_);
    rhs_chained_call->SetCamera(jittered_cam);

    (*rhs_chained_call)(core::view::AbstractCallRender::FnRender);

    // get rhs texture call
    std::shared_ptr<glowl::Texture2D> motion_vector_texture = m_dummy_motion_vector_tx_;
    CallTexture2D* mvt = m_motion_vector_texture_call_.CallAs<CallTexture2D>();
    if (mvt != NULL) {
        (*mvt)(0);
        motion_vector_texture = mvt->getData();
    }

    if (motion_vector_texture == nullptr) {
        return false;
    }

    // in first frame just use the same colorbuffer
    if (frames_ == 1) {
        //fbo_->getColorAttachment(0)->copy(fbo_->getColorAttachment(0).get(), old_fbo_->getColorAttachment(0).get());
    }

    glViewport(0, 0, w, h);
    lhs_input_fbo->bind();

    const glm::dmat4 shiftMx = glm::dmat4(lastViewProjMx_) * glm::inverse(glm::dmat4(viewProjMx_));

    const auto intrinsics = cam.get<core::view::Camera::PerspectiveParameters>();

    float frustrum_height = glm::tan(intrinsics.fovy * 0.5f) * 2.0f;
    temporal_aa_prgm_->use();

    temporal_aa_prgm_->setUniform("resolution", w, h);
    temporal_aa_prgm_->setUniform("lowResResolution", fbo_->getWidth(), fbo_->getHeight());
    temporal_aa_prgm_->setUniform("shiftMx", glm::mat4(shiftMx));
    temporal_aa_prgm_->setUniform("camCenter", cam.getPose().position);
    temporal_aa_prgm_->setUniform("camAspect", intrinsics.aspect.value());
    temporal_aa_prgm_->setUniform("frustumHeight", frustrum_height);
    temporal_aa_prgm_->setUniform("prevJitter", prev_jitter_);
    temporal_aa_prgm_->setUniform("curJitter", cur_jitter_);
    temporal_aa_prgm_->setUniform("numSamples", num_samples_);
    temporal_aa_prgm_->setUniform("frameIdx", frameIdx_);
    temporal_aa_prgm_->setUniform("samplingSequencePosition", samplingSequencePosition_);
    temporal_aa_prgm_->setUniform("viewProjMx", viewProjMx_);
    temporal_aa_prgm_->setUniform("viewMx", cam.getViewMatrix());
    temporal_aa_prgm_->setUniform("projMx", cam.getProjectionMatrix());

    glActiveTexture(GL_TEXTURE0);
    fbo_->bindColorbuffer(0);
    temporal_aa_prgm_->setUniform("curColorTex", 0);

    glActiveTexture(GL_TEXTURE1);
    motion_vector_texture->bindTexture();
    temporal_aa_prgm_->setUniform("motionVecTex", 1);

    texRead_->bindImage(0, GL_READ_ONLY);
    texWrite_->bindImage(1, GL_WRITE_ONLY);
    distTexRead_->bindImage(2, GL_READ_ONLY);
    distTexWrite_->bindImage(3, GL_WRITE_ONLY);
    old_lowres_color_read_->bindImage(4, GL_READ_ONLY);
    old_lowres_color_write_->bindImage(5, GL_WRITE_ONLY);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glUseProgram(0);

    texRead_.swap(texWrite_);
    distTexRead_.swap(distTexWrite_);
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
        old_fbo_->resize(oldWidth_ / 2, oldHeight_);
        camOffsets_.resize(2);

        for (int j = 0; j < 2; j++) {
            const float x = static_cast<float>(2 * j - 2 + 1) / (2 * static_cast<float>(oldWidth_));
            camOffsets_[j] = glm::vec3(x, 0.0f, 0.0f);
        }

        auto layout = texLayout_;
        layout.width = oldWidth_ / 2;
        layout.height = oldHeight_;
        const std::vector<uint32_t> zero_data((oldWidth_ / 2) * oldHeight_, 0);
        old_lowres_color_read_ = std::make_unique<glowl::Texture2D>("oldColorR", layout, zero_data.data());
        old_lowres_color_write_ = std::make_unique<glowl::Texture2D>("oldColorW", layout, zero_data.data());

    } else {
        shader_options_flags_->addDefinition("NONE");
        fbo_->resize(oldWidth_, oldHeight_);

        auto layout = texLayout_;
        layout.width = oldWidth_;
        layout.height = oldHeight_;
        const std::vector<uint32_t> zero_data(oldWidth_ * oldHeight_, 0);
        old_lowres_color_read_ = std::make_unique<glowl::Texture2D>("oldColorR", layout, zero_data.data());
        old_lowres_color_write_ = std::make_unique<glowl::Texture2D>("oldColorW", layout, zero_data.data());
    }

    viewProjMx_ = glm::mat4(1.0f);
    lastViewProjMx_ = glm::mat4(1.0f);

    texLayout_.width = oldWidth_;
    texLayout_.height = oldHeight_;
    const std::vector<uint32_t> zeroData(oldWidth_ * oldHeight_, 0); // uin32_t <=> RGBA8.
    texRead_ = std::make_unique<glowl::Texture2D>("texRead", texLayout_, zeroData.data());
    texWrite_ = std::make_unique<glowl::Texture2D>("texWrite", texLayout_, zeroData.data());


    distTexLayout_.width = oldWidth_;
    distTexLayout_.height = oldHeight_;
    const std::vector<float> posInit(2 * oldWidth_ * oldHeight_, std::numeric_limits<float>::lowest()); // RG32F
    distTexRead_ = std::make_unique<glowl::Texture2D>("distTexRead", distTexLayout_, posInit.data());
    distTexWrite_ = std::make_unique<glowl::Texture2D>("distTexWrite", distTexLayout_, posInit.data());

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
