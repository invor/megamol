/*
 * ArchVisCalls.h
 *
 * Copyright (C) 2020 by Universitaet Stuttgart (VISUS).
 * All rights reserved.
 */

#ifndef ARCH_VIS_CALLS_H_INCLUDED
#define ARCH_VIS_CALLS_H_INCLUDED

#include "mmcore/CallGeneric.h"

#include <memory>

#include "archvis/archvis.h"
#include "FEMModel.h"
#include "ScaleModel.h"

namespace megamol {
namespace archvis {

class ARCHVIS_API CallFEMData : public core::GenericVersionedCall<std::shared_ptr<FEMModel>, core::EmptyMetaData> {
public:
    inline CallFEMData() : core::GenericVersionedCall<std::shared_ptr<FEMModel>, core::EmptyMetaData>() {}
    ~CallFEMData() = default;

    static const char* ClassName(void) { return "CallFEMData"; }
    static const char* Description(void) { return "Call that transports..."; }
};

class ARCHVIS_API CallMSMData : public core::GenericVersionedCall<std::shared_ptr<ScaleModel>, core::EmptyMetaData> {
public:
    inline CallMSMData() : core::GenericVersionedCall<std::shared_ptr<ScaleModel>, core::EmptyMetaData>() {}
    ~CallMSMData() = default;

    static const char* ClassName(void) { return "CallFEMData"; }
    static const char* Description(void) { return "Call that transports..."; }
};

typedef megamol::core::factories::CallAutoDescription<CallFEMData> CallFEMDataDescription;
typedef megamol::core::factories::CallAutoDescription<CallMSMData> CallMSMDataDescription;

} // namespace archvis
} // namespace megamol


#endif // !ARCH_VIS_CALLS_H_INCLUDED
