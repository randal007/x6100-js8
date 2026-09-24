/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 receive, C interface for the dialog
 */

#include "js8_rx.h"

#include "classify.hpp"
#include "receiver.hpp"

#include <cstring>
#include <memory>
#include <new>
#include <string>

using namespace x6100::js8;

struct js8_rx {
    std::string               my_call;
    js8_rx_cb_t               cb{};
    std::unique_ptr<Receiver> receiver;
};

namespace {

void copy_str(char *dst, std::size_t cap, const std::string &src) {
    std::size_t n = std::min(cap - 1, src.size());
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

js8_rx_msg_t to_c(const RxFrame &f, const std::string &my_call) {
    js8_rx_msg_t m{};
    m.utc            = f.utc;
    m.snr            = (int16_t)f.snr;
    m.dt             = f.dt;
    m.freq_hz        = f.freq_hz;
    m.type           = (uint8_t)f.type;
    m.submode        = (uint8_t)f.mode;
    m.low_confidence = f.low_confidence;
    m.checksum       = (int8_t)f.checksum;

    auto mc    = classify(f.text, my_call);
    m.heartbeat = mc.heartbeat;
    m.cq        = mc.cq;
    m.to_me     = mc.to_me;
    m.to_group  = mc.to_group;
    copy_str(m.from, sizeof(m.from), mc.from);
    copy_str(m.to, sizeof(m.to), mc.to);
    copy_str(m.text, sizeof(m.text), f.text);
    return m;
}

} // namespace

extern "C" js8_rx_t *js8_rx_create(int input_rate, int submodes, const char *my_call, const js8_rx_cb_t *cb) {
    auto *rx = new (std::nothrow) js8_rx;
    if (!rx) return nullptr;
    if (my_call) rx->my_call = my_call;
    if (cb) rx->cb = *cb;

    Receiver::Config config;
    config.input_rate = input_rate;
    config.submodes   = submodes;

    Receiver::Callbacks callbacks;
    callbacks.on_frame = [rx](const RxFrame &f) {
        if (rx->cb.on_frame) {
            auto m = to_c(f, rx->my_call);
            rx->cb.on_frame(&m, rx->cb.ctx);
        }
    };
    callbacks.on_message = [rx](const RxFrame &f) {
        if (rx->cb.on_message) {
            auto m = to_c(f, rx->my_call);
            rx->cb.on_message(&m, rx->cb.ctx);
        }
    };
    callbacks.on_cycle_done = [rx](std::size_t n) {
        if (rx->cb.on_cycle_done) rx->cb.on_cycle_done((unsigned)n, rx->cb.ctx);
    };
    if (rx->cb.on_audio) {
        callbacks.on_audio = [rx](const float *samples, std::size_t n) {
            rx->cb.on_audio(samples, (unsigned)n, rx->cb.ctx);
        };
    }

    try {
        rx->receiver = std::make_unique<Receiver>(config, std::move(callbacks));
    } catch (...) {
        delete rx;
        return nullptr;
    }
    return rx;
}

extern "C" void js8_rx_feed(js8_rx_t *rx, const float *samples, unsigned n) {
    if (rx && samples && n) rx->receiver->feed(samples, n);
}

extern "C" void js8_rx_clear(js8_rx_t *rx) {
    if (rx) rx->receiver->clear_messages();
}

extern "C" void js8_rx_destroy(js8_rx_t *rx) {
    if (!rx) return;
    rx->receiver.reset();
    delete rx;
}
