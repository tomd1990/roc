/*
 * Copyright (c) 2015 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "roc_netio/udp_receiver_port.h"
#include "roc_core/log.h"
#include "roc_core/panic.h"
#include "roc_core/shared_ptr.h"
#include "roc_packet/address_to_str.h"
#include "roc_packet/ip_to_str.h"

namespace roc {
namespace netio {

UDPReceiverPort::UDPReceiverPort(ICloseHandler& close_handler,
                                 const packet::Address& address,
                                 uv_loop_t& event_loop,
                                 packet::IWriter& writer,
                                 packet::PacketPool& packet_pool,
                                 core::BufferPool<uint8_t>& buffer_pool,
                                 core::IAllocator& allocator)
    : BasicPort(allocator)
    , close_handler_(close_handler)
    , loop_(event_loop)
    , handle_initialized_(false)
    , multicast_group_joined_(false)
    , recv_started_(false)
    , closed_(false)
    , address_(address)
    , writer_(writer)
    , packet_pool_(packet_pool)
    , buffer_pool_(buffer_pool)
    , packet_counter_(0) {
}

UDPReceiverPort::~UDPReceiverPort() {
    if (handle_initialized_) {
        roc_panic(
            "udp receiver: receiver was not fully closed before calling destructor");
    }
}

const packet::Address& UDPReceiverPort::address() const {
    return address_;
}

bool UDPReceiverPort::open() {
    if (int err = uv_udp_init(&loop_, &handle_)) {
        roc_log(LogError, "udp receiver: uv_udp_init(): [%s] %s", uv_err_name(err),
                uv_strerror(err));
        return false;
    }

    handle_.data = this;
    handle_initialized_ = true;

    unsigned flags = 0;
    if (address_.multicast() && address_.port() > 0) {
        flags |= UV_UDP_REUSEADDR;
    }

    int bind_err = UV_EINVAL;
    if (address_.version() == 6) {
        bind_err = uv_udp_bind(&handle_, address_.saddr(), flags | UV_UDP_IPV6ONLY);
    }
    if (bind_err == UV_EINVAL || bind_err == UV_ENOTSUP) {
        bind_err = uv_udp_bind(&handle_, address_.saddr(), flags);
    }
    if (bind_err != 0) {
        roc_log(LogError, "udp receiver: uv_udp_bind(): [%s] %s", uv_err_name(bind_err),
                uv_strerror(bind_err));
        return false;
    }

    int addrlen = (int)address_.slen();
    if (int err = uv_udp_getsockname(&handle_, address_.saddr(), &addrlen)) {
        roc_log(LogError, "udp receiver: uv_udp_getsockname(): [%s] %s", uv_err_name(err),
                uv_strerror(err));
        return false;
    }

    if (addrlen != (int)address_.slen()) {
        roc_log(
            LogError,
            "udp receiver: uv_udp_getsockname(): unexpected len: got=%lu expected=%lu",
            (unsigned long)addrlen, (unsigned long)address_.slen());
        return false;
    }

    if (address_.multicast()) {
        if (!join_multicast_group_()) {
            return false;
        }
    }

    if (int err = uv_udp_recv_start(&handle_, alloc_cb_, recv_cb_)) {
        roc_log(LogError, "udp receiver: uv_udp_recv_start(): [%s] %s", uv_err_name(err),
                uv_strerror(err));
        return false;
    }

    roc_log(LogInfo, "udp receiver: opened port %s",
            packet::address_to_str(address_).c_str());

    recv_started_ = true;

    return true;
}

void UDPReceiverPort::async_close() {
    if (closed_) {
        return; // handle_closed() was already called
    }

    if (!handle_initialized_) {
        closed_ = true;
        close_handler_.handle_closed(*this);

        return;
    }

    roc_log(LogInfo, "udp receiver: closing port %s",
            packet::address_to_str(address_).c_str());

    if (recv_started_) {
        if (int err = uv_udp_recv_stop(&handle_)) {
            roc_log(LogError, "udp receiver: uv_udp_recv_stop(): [%s] %s",
                    uv_err_name(err), uv_strerror(err));
        }

        recv_started_ = false;
    }

    if (address_.multicast()) {
        leave_multicast_group_();
    }

    if (!uv_is_closing((uv_handle_t*)&handle_)) {
        uv_close((uv_handle_t*)&handle_, close_cb_);
    }
}

void UDPReceiverPort::close_cb_(uv_handle_t* handle) {
    roc_panic_if_not(handle);

    UDPReceiverPort& self = *(UDPReceiverPort*)handle->data;

    self.handle_initialized_ = false;

    roc_log(LogInfo, "udp receiver: closed port %s",
            packet::address_to_str(self.address_).c_str());

    self.closed_ = true;
    self.close_handler_.handle_closed(self);
}

void UDPReceiverPort::alloc_cb_(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
    roc_panic_if_not(handle);
    roc_panic_if_not(buf);

    UDPReceiverPort& self = *(UDPReceiverPort*)handle->data;

    core::SharedPtr<core::Buffer<uint8_t> > bp =
        new (self.buffer_pool_) core::Buffer<uint8_t>(self.buffer_pool_);

    if (!bp) {
        roc_log(LogError, "udp receiver: can't allocate buffer");

        buf->base = NULL;
        buf->len = 0;

        return;
    }

    if (size > bp->size()) {
        size = bp->size();
    }

    bp->incref(); // will be decremented in recv_cb_()

    buf->base = (char*)bp->data();
    buf->len = size;
}

void UDPReceiverPort::recv_cb_(uv_udp_t* handle,
                               ssize_t nread,
                               const uv_buf_t* buf,
                               const sockaddr* sockaddr,
                               unsigned flags) {
    roc_panic_if_not(handle);
    roc_panic_if_not(buf);

    UDPReceiverPort& self = *(UDPReceiverPort*)handle->data;

    packet::Address src_addr;
    if (sockaddr) {
        if (!src_addr.set_saddr(sockaddr)) {
            roc_log(
                LogError,
                "udp receiver: can't determine source address: num=%u dst=%s nread=%ld",
                self.packet_counter_, packet::address_to_str(self.address_).c_str(),
                (long)nread);
        }
    }

    core::SharedPtr<core::Buffer<uint8_t> > bp =
        core::Buffer<uint8_t>::container_of(buf->base);

    // one reference for incref() called from alloc_cb_()
    // one reference for the shared pointer above
    roc_panic_if(bp->getref() != 2);

    // decrement reference counter incremented in alloc_cb_()
    bp->decref();

    if (nread < 0) {
        roc_log(LogError, "udp receiver: network error: num=%u src=%s dst=%s nread=%ld",
                self.packet_counter_, packet::address_to_str(src_addr).c_str(),
                packet::address_to_str(self.address_).c_str(), (long)nread);
        return;
    }

    if (nread == 0) {
        if (!sockaddr) {
            // no more data for now
        } else {
            roc_log(LogTrace, "udp receiver: empty packet: num=%u src=%s dst=%s",
                    self.packet_counter_, packet::address_to_str(src_addr).c_str(),
                    packet::address_to_str(self.address_).c_str());
        }
        return;
    }

    if (!sockaddr) {
        roc_panic("udp receiver: unexpected null source address");
    }

    if (flags & UV_UDP_PARTIAL) {
        roc_log(LogDebug,
                "udp receiver:"
                " ignoring partial read: num=%u src=%s dst=%s nread=%ld",
                self.packet_counter_, packet::address_to_str(src_addr).c_str(),
                packet::address_to_str(self.address_).c_str(), (long)nread);
        return;
    }

    self.packet_counter_++;

    roc_log(LogTrace, "udp receiver: received packet: num=%u src=%s dst=%s nread=%ld",
            self.packet_counter_, packet::address_to_str(src_addr).c_str(),
            packet::address_to_str(self.address_).c_str(), (long)nread);

    if ((size_t)nread > bp->size()) {
        roc_panic("udp receiver: unexpected buffer size: got %ld, max %ld", (long)nread,
                  (long)bp->size());
    }

    packet::PacketPtr pp = new (self.packet_pool_) packet::Packet(self.packet_pool_);
    if (!pp) {
        roc_log(LogError, "udp receiver: can't allocate packet");
        return;
    }

    pp->add_flags(packet::Packet::FlagUDP);

    pp->udp()->src_addr = src_addr;
    pp->udp()->dst_addr = self.address_;

    pp->set_data(core::Slice<uint8_t>(*bp, 0, (size_t)nread));

    self.writer_.write(pp);
}

bool UDPReceiverPort::join_multicast_group_() {
    if (int err = uv_udp_set_membership(&handle_, packet::ip_to_str(address_).c_str(),
                                        NULL, UV_JOIN_GROUP)) {
        roc_log(LogError, "udp receiver: uv_udp_set_membership(): [%s] %s",
                uv_err_name(err), uv_strerror(err));
        return false;
    }

    return (multicast_group_joined_ = true);
}

void UDPReceiverPort::leave_multicast_group_() {
    if (!multicast_group_joined_) {
        return;
    }
    multicast_group_joined_ = false;

    if (int err = uv_udp_set_membership(&handle_, packet::ip_to_str(address_).c_str(),
                                        NULL, UV_LEAVE_GROUP)) {
        roc_log(LogError, "udp receiver: uv_udp_set_membership(): [%s] %s",
                uv_err_name(err), uv_strerror(err));
    }
}

} // namespace netio
} // namespace roc
