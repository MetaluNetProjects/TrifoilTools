/**
 * Copyright (c) 2026 metalu.net
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include <stdio.h>
#define FRAISE_DONT_OVERWRITE_PRINTF
#include "fraise.hpp"
#include "fraise_bus.hpp"
#include <cstring>


void FraisePoller::set_enable(int id, bool enable) {
    devices[id].enabled = enable;
    devices[id].detected = false;
}

void FraisePoller::service(FraiseBus *bus) {
    if(time_reached(print_timeout)) {
        print_timeout = make_timeout_time_ms(100);
        for(int i = 1; i <= FRAISE_ID_MAX; i++) {
            DeviceStatus &device = devices[i];
            if(device.sent_detected != device.detected) {
                device.sent_detected = device.detected;
                if(device.detected) printf("sC%02X\n", i);
                else printf("sc%02X\n", i);
            }
        }
    }

    if(bus->is_busy()) return;

    if(!time_reached(timeout)) return;
    timeout = make_timeout_time_ms(1);
    for(int i = 0; i <= FRAISE_ID_MAX; i++) {
        current_id = (current_id + 1) % (FRAISE_ID_MAX + 1);
        if(devices[current_id].enabled) {
            bus->poll(current_id);
            break;
        }
    }
}

void FraisePoller::detected(int id, bool is_detected) {
    if(id == current_id) devices[id].detected = devices[id].enabled && is_detected;
}

void FraisePoller::reset() {
    for(int id = 0; id <= FRAISE_ID_MAX; id++) {
        devices[id].sent_detected = devices[id].detected = devices[id].enabled = false;
    }
}

