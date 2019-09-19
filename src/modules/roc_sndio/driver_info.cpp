/*
 * Copyright (c) 2019 Roc authors
 *
 * This Sink Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "roc_sndio/driver_info.h"
#include "string.h"

namespace roc {
namespace sndio {

DriverInfo::DriverInfo(const char* driver_name) {
    strcpy(name, driver_name);
}

} // namespace sndio
} // namespace roc
