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

void FraiseReceiver::received(const char *data, int len) {
    fraise_receivechars(data, len);
}

static FraiseReceiver default_receiver;

FraiseBus::FraiseBus(FraiseCom *com, int id):
    com(com), id(id)
{
    set_receiver(&default_receiver);
}

#define ishex(x) ((x >= '0'&& x <='9') || (x >= 'A' && x <= 'F'))

bool FraiseBus::process_command(char *data) {
    int len = strlen(data);
    if(data[0] == '#') { // system
        switch(data[1]) {
        case 'R':
            printf("sID%02X\n", id);
            state = State::receive;
            break;
        case 'E':
            puts((const char*)(data + 2));
            break;
        case 'F':
            state = State::receive;
            printf("l quit bootloader mode\n");
            break;
        }
        return true;
    }
    else if(state == State::bootload) {
        if(data[0] == ':') { // push hex line to buffer
            queue_message(data, len);
        } else {
            send_to(-1, data, len);
        }
        return true;
    }
    else if(data[0] == '!') {
        switch(data[1]) {
        /*case 'b': {
            char buf[64];
            uint8_t count = 2;
            uint8_t bufcount = 0;
            while(count < len - 1) {
                buf[bufcount++] = gethexbyte(data + count);
                count += 2;
            }
            fraise_master_sendbytes_broadcast(buf, bufcount);
        }
        break;
        case 'B':
            fraise_master_sendchars_broadcast(data + 2);
            break;*/
        case 'F':
            send_to(0, data + 1, 3);
            state = State::bootload;
            boot_status.wait_ack = false;
            messages_queue.clear();
            rcv_len = 0;
            printf("l switch to bootloader mode\n");
            break;
        }
        return true;
    }
    else if(ishex(data[0]) && ishex(data[1])) { // normal output to fruit
        uint8_t dest_id = gethexbyte(data) - 128;
        if(dest_id == 0) {
            receiver->received(data + 2, len - 2);
        } else if(dest_id < FRAISE_ID_MAX) {
            send_queue.queue_message(dest_id, data + 2, len - 2);
        }
        return true;
    }

    return false;
}

void FraiseBus::send_to(int dest_id, const char *data, int len) {
    if(len >= 128) return;
    if(dest_id > FRAISE_ID_MAX) return;

    while(is_busy()) {
        if(state == State::poll) service(false);
    }
    uint8_t checksum = 0;
    char buffer[256];
    int buflen = 0;
    auto putc_sum = [&] (char c) {
        buffer[buflen++] = c;
        checksum += c;
    };

    if(dest_id >= 0) putc_sum(dest_id + 128);
    putc_sum(len);
    const char *p = data;
    while(len--) putc_sum(*p++);
    buffer[buflen++] = (-checksum) & 127;
    com->send(buffer, buflen);
}

void FraiseBus::poll(int id) {
    if(state == State::bootload) return;
    poll_id = id;
    if(poll_id > FRAISE_ID_MAX || poll_id <= 0) {
        poll_id = 0;
        return;
    }
    char buffer[2] = {(char)(poll_id + 128), 0};
    com->send(buffer, 2);
    rcv_len = 0;
    state = State::poll;
    poll_timeout = make_timeout_time_ms(FRAISE_POLL_TIMEOUT_MS);
}

bool FraiseBus::queue_message(const char *data, int len) {
    return messages_queue.queue_message(data, len);
}

static inline void send_zero(FraiseCom *com) {
    char zero = 0;
    com->send(&zero, 1);
}

void FraiseBus::send_message() {
    char buffer[128];
    int len = messages_queue.unqueue_message(buffer);
    if(len > 0) {
        send_to(-1, buffer, len);
    } else {
        char zero = 0;
        com->send(&zero, 1);
    }
}

static bool check_sum(char *data, int len) {
    int sum = 0;
    for(int i = 0; i < len; i++) sum += data[i];
    if((sum & 127) != 0) {
        printf("e bad sum %d: ", sum);
        for(int i = 0; i < len; i++) printf("%d ", data[i]);
        printf("\n");
        return false;
    }
    return true;
}

void FraiseBus::check_received() {
    if(!check_sum(rcv_buffer, rcv_len)) return;
    int dest_id = rcv_buffer[0];
    int len = rcv_buffer[1];
    rcv_buffer[rcv_len - 1] = 0; // clear checksum to have null-terminated string
    receiver->sent_to(dest_id, rcv_buffer + 2, len);
    if(dest_id == id) receiver->received(rcv_buffer + 2, len);
}

void FraiseBus::check_poll_received() {
    if(!check_sum(rcv_buffer, rcv_len)) return;
    int len = rcv_buffer[0];
    rcv_buffer[rcv_len - 1] = 0; // clear checksum to have null-terminated string
    receiver->detected(poll_id, true);
    receiver->received_from(poll_id, rcv_buffer + 1, len);
}

void FraiseBus::check_poll_timeout() {
    if(time_reached(poll_timeout) && state == State::poll) {
        poll_timeout = 0;
        receiver->detected(poll_id, false);
        state = State::receive;
    }
}

void FraiseBus::service(bool enable_send) {
    while(com->is_readable()) {
        char c = com->getc();
        switch(state) {
            case State::poll: {
                poll_timeout = make_timeout_time_ms(1);
                if(rcv_len < 130) rcv_buffer[rcv_len++] = c;
                else {
                    state = State::receive;
                    break;
                }
                if(rcv_len == 1 && c == 0) { // empty answer
                    receiver->detected(poll_id, true);
                    state = State::receive;
                    break;
                }
                if(rcv_len > 2 && rcv_len == rcv_buffer[0] + 2) {
                    check_poll_received();
                    state = State::receive;
                    break;
                }
            }
            break;
            case State::receive: {
                if(c >= 128) {
                    rcv_len = 0;
                    rcv_buffer[rcv_len++] = c - 128;
                } else if(rcv_len > 0 && rcv_len < 131) {
                    if(rcv_len == 1 && c == 0 && rcv_buffer[0] == id) send_message();
                    else {
                        rcv_buffer[rcv_len++] = c;
                        if(rcv_len > 3 && rcv_len == rcv_buffer[1] + 3) {
                            check_received();
                            rcv_len = 0;
                        }
                    }
                }
            }
            break;
            case State::bootload:
                if(c == 'X') {
                    boot_status.wait_ack = false;
                    boot_status.nb_trials = 0;
                    rcv_len = 0;
                }
                printf("b%c\n", c);
            break;
            default: ;
        }
    }

    check_poll_timeout();

    if(enable_send && state == State::receive) {
        char buffer[128];
        char dest_id;
        int len = send_queue.unqueue_message(dest_id, buffer);
        if(len > 0) {
            send_to(dest_id, buffer, len);
        }
    }

    if(state == State::bootload) {
        if(!boot_status.wait_ack) {
            if(rcv_len == 0) {
                rcv_len = messages_queue.unqueue_message(rcv_buffer);
            }
            if(rcv_len > 0) {
                send_to(-1, rcv_buffer, rcv_len);
                boot_status.wait_ack = true;
                boot_status.timeout = make_timeout_time_ms(100);
            } else boot_status.timeout = at_the_end_of_time;
        } else if(time_reached(boot_status.timeout)) {
            if(++boot_status.nb_trials > boot_status.max_trials) {
                printf("bE\n");
                messages_queue.clear();
            } else printf("l retry %d\n", boot_status.nb_trials);
            boot_status.wait_ack = false;
            boot_status.timeout = at_the_end_of_time;
        }
    }
}

void FraiseBus::set_receiver(FraiseReceiver *r) {
    receiver = r;
}

