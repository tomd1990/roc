/*
 * Copyright (c) 2019 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "roc_packet/ip_to_str.h"
#include "roc_core/log.h"

namespace roc {
namespace packet {

ip_to_str::ip_to_str(const Address& addr) {
    buffer_[0] = '\0';

    switch (addr.version()) {
    case 4: {
        if (!addr.get_ip(buffer_, sizeof(buffer_))) {
            roc_log(LogError, "ip_str_formatter: can't format ip");
        }

        break;
    }
    case 6: {
        buffer_[0] = '[';

        if (!addr.get_ip(buffer_ + 1, sizeof(buffer_) - 1)) {
            roc_log(LogError, "ip_str_formatter: can't format ip");
        }

        const size_t blen = strlen(buffer_);

        buffer_[blen] = ']';
        buffer_[blen + 1] = '\0';

        break;
    }
    default:
        strcpy(buffer_, "none");
        break;
    }
}

} // namespace packet
} // namespace roc
