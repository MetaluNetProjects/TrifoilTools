/**
 * Copyright (c) 2026 metalu.net
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "pico/stdlib.h"
#include <stdio.h>
#include "ringbuffer.hpp"

#define FRAISE_ID_MAX 127
#define FRAISE_POLL_TIMEOUT_MS 10

class FraiseCom {
public:
    virtual bool is_readable() = 0;
    virtual char getc() = 0; 
    virtual bool is_writable() = 0;
    virtual void putc(char c) = 0;
    virtual bool tx_in_progress() = 0;
    virtual void set_drive(bool drive) = 0;
    virtual void send(const char *data, uint8_t len) = 0; // set drive, send bytes, and clear drive
    //virtual void set_irq_handler(irq_handler_t handler) = 0;
    //virtual void set_irqs_enabled (bool rx_has_data, bool tx_needs_data) = 0;
};

class FraiseUart : public FraiseCom {
public:
    FraiseUart(int txpin, int rxpin, int drvpin, bool drvlevel);
    virtual bool is_readable() override;
    virtual char getc() override; 
    virtual bool is_writable() override;
    virtual void putc(char c) override;
    virtual bool tx_in_progress() override;
    virtual void set_drive(bool drive) override;
    virtual void send(const char *data, uint8_t len) override; // set drive, send bytes, and clear drive
    //virtual void set_irq_handler(irq_handler_t handler) override;
    //virtual void set_irqs_enabled (bool rx_has_data, bool tx_needs_data) override;
private:
    uart_inst_t *uart = uart0;
    int drive_pin;
    bool drive_level;
    char buffer[256]{0};
    int buffer_length = 0;
    int buffer_sent = 0;
    bool send_in_progress = false;
};

struct FraiseReceiver {
    virtual void sent_to(int dest_id, const char *data, int len) {}
    virtual void received_from(int src_id, const char *data, int len) {}
    virtual void received(const char *data, int len);
    virtual void detected(int src_id, bool is_detected) {}
};

class FraiseBus {
private:
    FraiseCom *com = nullptr;
    int id = -1;
    FraiseReceiver *receiver = nullptr;
    enum class State {poll, receive, send, bootload} state = State::receive;
    char rcv_buffer[256]{0};
    int rcv_len = -1;
    RingBuffer<char, 1024> messages_queue;
    RingBuffer<char, 1024> send_queue;
    int poll_id = 0;
    absolute_time_t poll_timeout = at_the_end_of_time;
    void send_message();
    void check_received();
    void check_poll_received();
    void check_poll_timeout();
    struct BootStatus {
        static const int max_trials = 10;
        bool wait_ack = false;
        absolute_time_t timeout = at_the_end_of_time;
        int nb_trials = 0;
    } boot_status;

public:
    FraiseBus(FraiseCom *com = nullptr, int id = -1);
    bool process_command(char *data); // e.g from stdin; data = null-terminated string
    void send_to(int dest_id, const char *data, int len); // start with 8-bit address if (dest_id >= 0)
    void poll(int poll_id);
    bool queue_message(const char *data, int len);
    void service(bool enable_send = true);
    void set_receiver(FraiseReceiver *r);
    bool is_busy() { 
        return com->tx_in_progress() || (state == State::poll) || (state == State::send);
    }
    State get_state() { return state; }
    const char *get_state_name() {
        switch(state) {
        case State::poll: return "poll"; break;
        case State::receive: return "receive"; break;
        case State::send: return "send"; break;
        case State::bootload: return "bootload"; break;
        default: return"?";
        }
    }
};

class FraisePoller {
private:
    int current_id = 0;
    absolute_time_t timeout = 0;
    absolute_time_t print_timeout = 0;
    struct DeviceStatus {
        bool enabled;
        bool detected;
        bool sent_detected;
    };
    DeviceStatus devices[FRAISE_ID_MAX + 1]{0};
public:
    void set_enable(int id, bool enable);
    void service(FraiseBus *bus);
    void detected(int id, bool is_detected);
    void reset();
};

FraiseBus *fraise_main_bus();
