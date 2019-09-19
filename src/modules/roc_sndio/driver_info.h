/*
 * Copyright (c) 2019 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! @file roc_sndio/driver_info.h
//! @brief Driver info interface.

#ifndef ROC_SNDIO_DRIVER_INFO_H_
#define ROC_SNDIO_DRIVER_INFO_H_

namespace roc {
namespace sndio {

//! Driver info interface.
struct DriverInfo {
public:
    DriverInfo();

    //! Parameterized Constructor initializes name
    explicit DriverInfo(const char* driver_name);

    //! Placeholder for the driver name
    char name[16];
};

} // namespace sndio
} // namespace roc

#endif // ROC_SNDIO_DRIVER_INFO_H_
