/*
 * TriSoup.cpp
 *
 * Copyright (C) 2010 by Sebastian Grottel
 * Copyright (C) 2009-2010 by VISUS (Universitaet Stuttgart)
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "TriSoup.h"
#include "api/MegaMolCore.std.h"
#include "ModuleAutoDescription.h"
#include "vislib/vislibversion.h"
#include "vislib/Log.h"
#include "vislib/ThreadSafeStackTrace.h"
#include "TriSoupRenderer.h"
#include "CallTriMeshData.h"
#include "TriSoupDataSource.h"
#include "WavefrontObjDataSource.h"
#include "BlockVolumeMesh.h"
#include "CallBinaryVolumeData.h"
#include "volumetrics/VoluMetricJob.h"
#include "OSCBFix.h"
#include "CoordSysMarker.h"
#include "TrackerRendererTransform.h"



/*
 * mmplgPluginAPIVersion
 */
TRISOUP_API int mmplgPluginAPIVersion(void) {
    return 100;
}


/*
 * mmplgPluginName
 */
TRISOUP_API const char * mmplgPluginName(void) {
    return "TriSoup";
}


/*
 * mmplgPluginDescription
 */
TRISOUP_API const char * mmplgPluginDescription(void) {
    return "Plugin for rendering TriSoup mesh data";
}


/*
 * mmplgCoreCompatibilityValue
 */
TRISOUP_API const void * mmplgCoreCompatibilityValue(void) {
    static const mmplgCompatibilityValues compRev = {
        sizeof(mmplgCompatibilityValues),
        MEGAMOL_CORE_COMP_REV,
        VISLIB_VERSION_REVISION
    };
    return &compRev;
}


/*
 * mmplgModuleCount
 */
TRISOUP_API int mmplgModuleCount(void) {
    return 8;
}


/*
 * mmplgModuleDescription
 */
TRISOUP_API void* mmplgModuleDescription(int idx) {
    switch (idx) {
        case 0: return new megamol::core::ModuleAutoDescription<megamol::trisoup::TriSoupRenderer>();
        case 1: return new megamol::core::ModuleAutoDescription<megamol::trisoup::TriSoupDataSource>();
        case 2: return new megamol::core::ModuleAutoDescription<megamol::trisoup::WavefrontObjDataSource>();
        case 3: return new megamol::core::ModuleAutoDescription<megamol::trisoup::BlockVolumeMesh>();
        case 4: return new megamol::core::ModuleAutoDescription<megamol::trisoup::volumetrics::VoluMetricJob>();
        case 5: return new megamol::core::ModuleAutoDescription<megamol::quartz::OSCBFix>();
        case 6: return new megamol::core::ModuleAutoDescription<megamol::trisoup::CoordSysMarker>();
        case 7: return new megamol::core::ModuleAutoDescription<megamol::trisoup::TrackerRendererTransform>();
    }
    return NULL;
}


/*
 * mmplgCallCount
 */
TRISOUP_API int mmplgCallCount(void) {
    return 2;
}


/*
 * mmplgCallDescription
 */
TRISOUP_API void* mmplgCallDescription(int idx) {
    switch (idx) {
        case 0: return new megamol::trisoup::CallTriMeshDataDescription();
        case 1: return new megamol::trisoup::CallBinaryVolumeDataDescription();
    }
    return NULL;
}


/*
 * mmplgConnectStatics
 */
TRISOUP_API bool mmplgConnectStatics(int which, void* value) {
    switch (which) {

        case 1: // vislib::log
            vislib::sys::Log::DefaultLog.SetLogFileName(static_cast<const char*>(NULL), false);
            vislib::sys::Log::DefaultLog.SetLevel(vislib::sys::Log::LEVEL_NONE);
            vislib::sys::Log::DefaultLog.SetEchoTarget(new vislib::sys::Log::RedirectTarget(static_cast<vislib::sys::Log*>(value)));
            vislib::sys::Log::DefaultLog.SetEchoLevel(vislib::sys::Log::LEVEL_ALL);
            vislib::sys::Log::DefaultLog.EchoOfflineMessages(true);
            return true;

        case 2: // vislib::stacktrace
            return vislib::sys::ThreadSafeStackTrace::Initialise(
                *static_cast<const vislib::SmartPtr<vislib::StackTrace>*>(value), true);

    }
    return false;
}
