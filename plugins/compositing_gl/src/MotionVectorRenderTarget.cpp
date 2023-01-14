#include "MotionVectorRenderTarget.h"

#include "compositing_gl/CompositingCalls.h"

megamol::compositing_gl::MotionVectorRenderTarget::MotionVectorRenderTarget()
        : SimpleRenderTarget()
        , m_motion_vector_render_target("Motion Vector", "Access the motion vector render target texture") {
    this->m_motion_vector_render_target.SetCallback(
        CallTexture2D::ClassName(), "GetData", &MotionVectorRenderTarget::getMotionVectorRenderTarget);
    this->m_motion_vector_render_target.SetCallback(
        CallTexture2D::ClassName(), "GetMetaData", &MotionVectorRenderTarget::getMetaDataCallback);
    this->MakeSlotAvailable(&this->m_motion_vector_render_target);
}

bool megamol::compositing_gl::MotionVectorRenderTarget::create() {
    SimpleRenderTarget::create();
    m_GBuffer->createColorAttachment(GL_RGB32F, GL_RGB, GL_FLOAT); // motion vector texture
    return true;
}

bool megamol::compositing_gl::MotionVectorRenderTarget::Render(mmstd_gl::CallRender3DGL& call) {
    SimpleRenderTarget::Render(call);
    return true;
}

bool megamol::compositing_gl::MotionVectorRenderTarget::getMotionVectorRenderTarget(core::Call& caller) {
    auto ct = dynamic_cast<CallTexture2D*>(&caller);

    if (ct == NULL)
        return false;

    ct->setData(m_GBuffer->getColorAttachment(2), this->m_version);

    return true;
}

bool megamol::compositing_gl::MotionVectorRenderTarget::getMetaDataCallback(core::Call& caller) {
    return true;
}
