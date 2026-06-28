/**
 * Copyright (c) 2024 metalu.net
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "pico/stdlib.h"
#include <stdio.h>
//#include "fraise.h"
#include "fraise_uart.h"

class FraiseUart {
public:
    virtual char getc(); 
    virtual void putc (char c);
    virtual bool is_readable();
    virtual bool is_writable();
    virtual void tx_wait_blocking();
    virtual bool tx_in_progress();
    virtual void set_drive(bool drive);
};

class FraiseBusInternal {
protected:
    void send_raw(const char *data, uint8_t len); // 7bit data only; will append '\n'
    FraiseUart *uart = nullptr;
    int id = -1;
}

class FraiseBus : public FraiseBusInternal {
public:
    FraiseBus(FraiseUart *uart = nullptr, int id = -1);
    bool process_command(char *data); // e.g from stdin; data = null-terminated string
    void send_to(int dest_id, const char *data, int len); // start with 8bit address
    void poll(int poll_id);
    bool queue_for_polling(const char *data, int len);
    virtual void received_from(int src_id, const char *data, int len);
    virtual void received(const char *data, int len);
};

