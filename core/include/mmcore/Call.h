/*
 * Call.h
 *
 * Copyright (C) 2008 by Universitaet Stuttgart (VIS). 
 * Alle Rechte vorbehalten.
 */

#ifndef MEGAMOLCORE_CALL_H_INCLUDED
#define MEGAMOLCORE_CALL_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include <memory>
#ifdef PROFILING
#include <vector>
#include <array>
#include <map>
#include <string>
#include "PerformanceQueryManager.h"
#include "PerformanceHistory.h"
#endif

#include "mmcore/api/MegaMolCore.std.h"

namespace megamol {
namespace core {

    /** Forward declaration of description and slots */
    class CalleeSlot;
    class CallerSlot;
    namespace factories {
        class CallDescription;
    }


    /**
     * Base class of rendering graph calls
     */
    class MEGAMOLCORE_API Call : public std::enable_shared_from_this<Call> {
    public:

        /** The description generates the function map */
        friend class ::megamol::core::factories::CallDescription;

        /** Callee slot is allowed to map functions */
        friend class CalleeSlot;

        /** The caller slot registeres itself in the call */
        friend class CallerSlot;

        /** Shared ptr type alias */
        using ptr_type = std::shared_ptr<Call>;

        /** Shared ptr type alias */
        using const_ptr_type = std::shared_ptr<const Call>;

        /** Weak ptr type alias */
        using weak_ptr_type = std::weak_ptr<Call>;

        /** Ctor. */
        Call(void);

        /** Dtor. */
        virtual ~Call(void);

        /**
         * Calls function 'func'.
         *
         * @param func The function to be called.
         *
         * @return The return value of the function.
         */
        bool operator()(unsigned int func = 0);

        /**
         * Answers the callee slot this call is connected to.
         *
         * @return The callee slot this call is connected to.
         */
        inline const CalleeSlot * PeekCalleeSlot(void) const {
            return this->callee;
        }

        CalleeSlot* PeekCalleeSlotNoConst() const { return this->callee; }

        /**
         * Answers the caller slot this call is connected to.
         *
         * @return The caller slot this call is connected to.
         */
        inline const CallerSlot * PeekCallerSlot(void) const {
            return this->caller;
        }

        CallerSlot* PeekCallerSlotNoConst() const { return this->caller; }

        inline void SetClassName(const char *name) {
            this->className = name;
        }

        inline const char * ClassName() const {
            return this->className;
        }

#ifdef PROFILING
        static uint32_t GetSampleHistoryLength() {
            return PerformanceHistory::buffer_length;
        }
        inline double GetLastCPUTime(uint32_t func) const {
            if (func < callback_names.size())
                return cpu_history[func].last_value();
            else
                return -1.0;
        }
        inline double GetAverageCPUTime(uint32_t func) const {
            if (func < callback_names.size())
                return cpu_history[func].average();
            else
                return -1.0;
        }
        inline uint32_t GetNumCPUSamples(uint32_t func) const {
            if (func < callback_names.size())
                return cpu_history[func].samples();
            else
                return 0;
        }
        inline double GetLastGPUTime(uint32_t func) const {
            if (func < callback_names.size())
                return gpu_history[func].last_value();
            else
                return -1.0;
        }
        inline double GetAverageGPUTime(uint32_t func) const {
            if (func < callback_names.size())
                return gpu_history[func].average();
            else
                return -1.0;
        }
        inline uint32_t GetNumGPUSamples(uint32_t func) const {
            if (func < callback_names.size())
                return gpu_history[func].samples();
            else
                return 0;
        }

        inline uint32_t GetFuncCount() const {
            /// XXX assert(last_cpu_time.size() == avg_cpu_time.size() == num_cpu_time_samples.size() == last_gpu_time.size() ==
            ///       avg_gpu_time.size() == num_gpu_time_samples.size());
            return static_cast<uint32_t>(callback_names.size());
        }
        inline std::string GetFuncName(uint32_t i) const {
            if (i < callback_names.size()) {
                return callback_names[i];
            } else {
                return "out of bounds";
            }
        }
        static void InitializeQueryManager() {
            if (qm == nullptr) {
                qm = new PerformanceQueryManager();
            }
        }
        static void CollectGPUPerformance() {
            if (qm != nullptr) {
                qm->Collect();
            }
        }
#endif

    private:

        /** The callee connected by this call */
        CalleeSlot *callee;

        /** The caller connected by this call */
        CallerSlot *caller;

        const char *className;

        /** The function id mapping */
        unsigned int *funcMap;

#ifdef PROFILING
        friend class MegaMolGraph;
        friend class PerformanceQueryManager;

        void setProfilingInfo(std::vector<std::string> names, bool usesGL) {
            callback_names = std::move(names);
            cpu_history.resize(callback_names.size());
            gpu_history.resize(callback_names.size());
            uses_gl = usesGL;
            if (usesGL) {
                qm->AddCall(this);
            }
        }

        std::vector<PerformanceHistory> cpu_history;
        std::vector<PerformanceHistory> gpu_history;

        std::vector<std::string> callback_names;
        bool uses_gl = false;

        // todo these are so slow, we need double buffering all the same. best wrap starting_call and func inside the query and vary across the current frame

        // there is only one query. it needs to be fetched and the result assigned
        // to starting_call at the end of the frame.
        static PerformanceQueryManager *qm;
        // who can use the query this frame?
#endif PROFILING

    };


} /* end namespace core */
} /* end namespace megamol */

#endif /* MEGAMOLCORE_CALL_H_INCLUDED */
