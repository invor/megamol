/*
 * InteractionRenderTarget.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VISUS).
 * All rights reserved.
 */

#pragma once

#include "SimpleRenderTarget.h"

namespace megamol {
namespace compositing_gl {

class MotionVectorRenderTarget : public SimpleRenderTarget {
public:
    /**
     * Answer the name of this module.
     *
     * @return The name of this module.
     */
    static const char* ClassName(void) {
        return "MotionVectorRenderTarget";
    }

    /**
     * Answer a human readable description of this module.
     *
     * @return A human readable description of this module.
     */
    static const char* Description(void) {
        return "Binds a FBO with color, normal, depth and motion vector render targets.";
    }

    MotionVectorRenderTarget();
    ~MotionVectorRenderTarget() = default;

protected:
    /**
     * Implementation of 'Create'.
     *
     * @return 'true' on success, 'false' otherwise.
     */
    bool create();

    /**
     * The render callback.
     *
     * @param call The calling call.
     *
     * @return The return value of the function.
     */
    bool Render(mmstd_gl::CallRender3DGL& call);

    /**
     *
     */
    bool getMotionVectorRenderTarget(core::Call& caller);

    /**
     *
     */
    bool getMetaDataCallback(core::Call& caller);

private:
    core::CalleeSlot m_motion_vector_render_target;
};

} // namespace compositing_gl
} // namespace megamol
