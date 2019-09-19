/*
 * Copyright (c) 2019 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! @file roc_sndio/backend_dispatcher.h
//! @brief Backend dispatcher.

#ifndef ROC_SNDIO_BACKEND_DISPATCHER_H_
#define ROC_SNDIO_BACKEND_DISPATCHER_H_

#include "roc_core/array.h"
#include "roc_core/iallocator.h"
#include "roc_core/noncopyable.h"
#include "roc_core/shared_ptr.h"
#include "roc_core/singleton.h"
#include "roc_sndio/ibackend.h"
#include "roc_sndio/isink.h"
#include "roc_sndio/isource.h"

namespace roc {
namespace sndio {

//! Backend dispatcher.
class BackendDispatcher : public core::NonCopyable<> {
public:
    //! Get instance.
    static BackendDispatcher& instance() {
        return core::Singleton<BackendDispatcher>::instance();
    }

    //! Set internal buffer size for all backends that need it.
    void set_frame_size(size_t size);

    //! Create and open a sink.
    ISink* open_sink(core::IAllocator& allocator,
                     const char* driver,
                     const char* output,
                     const Config& config);

    //! Create and open a source.
    ISource* open_source(core::IAllocator& allocator,
                         const char* driver,
                         const char* input,
                         const Config& config);

    //! Append supported drivers from all registered backends to Array
    void get_drivers(core::Array<DriverInfo>& arr, IBackend::ProbeFlags driver_type);

private:
    friend class core::Singleton<BackendDispatcher>;

    BackendDispatcher();

    IBackend* select_backend_(const char* driver, const char* inout, int flags);
    IBackend* probe_backends_(const char* driver, const char* inout, int flags);

    void add_backend_(IBackend& backend);

    enum { MaxBackends = 8 };

    IBackend* backends_[MaxBackends];
    size_t n_backends_;
};

} // namespace sndio
} // namespace roc

#endif // ROC_SNDIO_BACKEND_DISPATCHER_H_
