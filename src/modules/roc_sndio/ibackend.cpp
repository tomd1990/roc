/*
 * Copyright (c) 2019 Roc authors
 *
 * This Backend Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "roc_sndio/ibackend.h"

namespace roc {
namespace sndio {

IBackend::~IBackend() {
}

bool IBackend::add_driver_uniq(core::Array<DriverInfo>& arr, const char* driver_name) {
    for (size_t n = 0; n < arr.size(); n++) {
        if (strcmp(driver_name, arr[n].name) == 0) {
            return false;
        }
    }
    if (arr.grow(arr.size() + 1)) {
        DriverInfo new_driver(driver_name);
        arr.push_back(new_driver);
        return true;
    } else {
        return false;
    }
}

} // namespace sndio
} // namespace roc
