/*
 * Copyright (c) 2015 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>

#include "roc_core/log.h"
#include "roc_core/scoped_lock.h"
#include "roc_core/unique_ptr.h"
#include "roc_sndio/sox_backend.h"
#include "roc_sndio/sox_sink.h"
#include "roc_sndio/sox_source.h"

namespace roc {
namespace sndio {

namespace {

const char* driver_priorities[] = {
    //
    "waveaudio",  // windows
    "coreaudio",  // macos
    "pulseaudio", // linux
    "alsa",       // linux
    "sndio",      // openbsd
    "sunaudio",   // solaris
    "oss",        // unix
    "ao",         // cross-platform fallback, no capture
    "null"        //
};

const char* select_default_driver() {
    for (size_t n = 0; n < ROC_ARRAY_SIZE(driver_priorities); n++) {
        const char* driver = driver_priorities[n];

        if (sox_find_format(driver, sox_false)) {
            return driver;
        }
    }

    return NULL;
}

const char* select_default_device(const char* driver) {
    const sox_format_handler_t* format = sox_find_format(driver, sox_false);
    if (!format) {
        return NULL;
    }

    if (format->flags & SOX_FILE_DEVICE) {
        return "default";
    }

    return "-";
}

bool select_defaults(const char*& driver, const char*& device) {
    if (!device) {
        if (!driver) {
            if (!(driver = select_default_driver())) {
                return false;
            }
        }
        if (!(device = select_default_device(driver))) {
            return false;
        }
    }
    return true;
}

void log_handler(unsigned sox_level,
                 const char* filename,
                 const char* format,
                 va_list args) {
    LogLevel level;

    switch (sox_level) {
    case 0:
    case 1: // fail
        level = LogError;
        break;

    case 2: // warn
        level = LogInfo;
        break;

    case 3: // info
        level = LogDebug;
        break;

    default: // debug, debug more, debug most
        level = LogTrace;
        break;
    }

    if (level > core::Logger::instance().level()) {
        return;
    }

    char message[256] = {};
    vsnprintf(message, sizeof(message) - 1, format, args);

    roc_log(level, "sox: %s: %s", filename, message);
}

} // namespace

SoxBackend::SoxBackend() {
    roc_log(LogDebug, "initializing sox backend");

    sox_init();

    sox_get_globals()->verbosity = 100;
    sox_get_globals()->output_message_handler = log_handler;
}

void SoxBackend::set_frame_size(size_t size) {
    core::Mutex::Lock lock(mutex_);

    sox_get_globals()->bufsiz = size * sizeof(sox_sample_t);
}

bool SoxBackend::probe(const char* driver, const char* inout, int flags) {
    if (!select_defaults(driver, inout)) {
        return false;
    }

    const sox_format_handler_t* handler = sox_write_handler(inout, driver, NULL);
    if (!handler) {
        return false;
    }

    if (handler->flags & SOX_FILE_DEVICE) {
        if ((flags & ProbeDevice) == 0) {
            return false;
        }
    } else {
        if ((flags & ProbeFile) == 0) {
            return false;
        }
    }

    return true;
}

ISink* SoxBackend::open_sink(core::IAllocator& allocator,
                             const char* driver,
                             const char* output,
                             const Config& config) {
    if (!select_defaults(driver, output)) {
        return NULL;
    }

    core::UniquePtr<SoxSink> sink(new (allocator) SoxSink(allocator, config), allocator);
    if (!sink) {
        return NULL;
    }

    if (!sink->valid()) {
        return NULL;
    }

    if (!sink->open(driver, output)) {
        return NULL;
    }

    return sink.release();
}

ISource* SoxBackend::open_source(core::IAllocator& allocator,
                                 const char* driver,
                                 const char* input,
                                 const Config& config) {
    if (!select_defaults(driver, input)) {
        return NULL;
    }

    core::UniquePtr<SoxSource> source(new (allocator) SoxSource(allocator, config),
                                      allocator);
    if (!source) {
        return NULL;
    }

    if (!source->valid()) {
        return NULL;
    }

    if (!source->open(driver, input)) {
        return NULL;
    }

    return source.release();
}

void SoxBackend::get_drivers(core::Array<DriverInfo>& arr, ProbeFlags driver_type) {
    const sox_format_tab_t* formats = sox_get_format_fns();
    char const* const* format_names;
    for (size_t n = 0; formats[n].fn; n++) {
        sox_format_handler_t const* handler = formats[n].fn();
        if ((!(handler->flags & SOX_FILE_DEVICE)) & (size_t)(ProbeFile & driver_type) >> 2
            || ((handler->flags & SOX_FILE_DEVICE) && !(handler->flags & SOX_FILE_PHONY))
                & (size_t)(ProbeDevice & driver_type) >> 3) {
            for (format_names = handler->names; *format_names; ++format_names) {
                if (!strchr(*format_names, '/')) {
                    add_driver_uniq(arr, *format_names);
                }
            }
        }
    }
}

} // namespace sndio
} // namespace roc
