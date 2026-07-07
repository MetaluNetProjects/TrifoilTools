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
#include <hardware/uart.h>
#include <cstring>

#define FRAISE_UART_BAUDRATE 250000

FraiseUart::FraiseUart(int txpin, int rxpin, int drvpin, bool drvlevel):
    drive_pin(drvpin), drive_level(drvlevel)
{
    switch(txpin) {
/*    case 0:
    case 12:
    case 16:
    case 28:
#if !PICO_RP2040
    case 2:
    case 14:
    case 18:
    case 30:
    case 32:
    case 34:
    case 44:
    case 46:
#endif
        uart = uart0;
        break;
*/

    case 4:
    case 8:
    case 20:
    case 24:
#if !PICO_RP2040
    case 6:
    case 10:
    case 22:
    case 26:
    case 36:
    case 38:
    case 40:
    case 42:
#endif
        uart = uart1;
        break;

    default:
        uart = uart0;
    }

    uart_init(uart, FRAISE_UART_BAUDRATE);
    gpio_set_function(txpin, UART_FUNCSEL_NUM(uart, UART_TX_PIN));
    gpio_set_function(rxpin, UART_FUNCSEL_NUM(uart, UART_RX_PIN));
    gpio_init(drive_pin);
    gpio_set_dir(drive_pin, GPIO_OUT);
    gpio_put(drive_pin, !drive_level);
}

bool FraiseUart::is_readable() {
    return uart_is_readable(uart);
}

char FraiseUart::getc() {
    return uart_getc(uart);
}

bool FraiseUart::is_writable() {
    return uart_is_writable(uart);
}

void FraiseUart::putc(char c) {
    uart_putc_raw(uart, c);
}

bool FraiseUart::tx_in_progress() {
    return (uart_get_hw(uart)->fr & UART_UARTFR_BUSY_BITS);
}

void FraiseUart::set_drive(bool drive) {
    if(!drive) {
        while(tx_in_progress()) tight_loop_contents();
    }
    gpio_put(drive_pin, drive_level ? drive : !drive);
}

void FraiseUart::send(const char *data, uint8_t len) {
    set_drive(true);
    for(int i = 0; i < len; i++) putc(data[i]);
    //while(tx_in_progress()) tight_loop_contents();
    //set_drive(false);
}

//void FraiseUart::set_irq_handler(irq_handler_t handler) {}
//void FraiseUart::set_irqs_enabled (bool rx_has_data, bool tx_needs_data) {}

// --------------------------------------------------------------------------- //

void FraiseReceiver::sent_to(int dest_id, const char *data, int len) {}
void FraiseReceiver::received_from(int src_id, const char *data, int len) {}
void FraiseReceiver::received(const char *data, int len) {
    fraise_receivechars(data, len);
}
void FraiseReceiver::detected(int src_id) {}

// --------------------------------------------------------------------------- //
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
        /*case 'S':
            if(gethexbyte(data + 2) == 0) printf("sC00\n");
            //fraise_master_set_poll(gethexbyte(data + 2), true);
            break;
        case 'C':
            if(gethexbyte(data + 2) == 0) printf("sc00\n");
            //fraise_master_set_poll(gethexbyte(data + 2), false);
            break;
        case 'i':
            //fraise_master_reset_polls();
            break;*/
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
            //fraise_receivechars(data + 2, len - 2);
            receiver->received(data + 2, len - 2);
        } else if(dest_id < FRAISE_ID_MAX) {
            send_to(dest_id, data + 2, len - 2);
        }
        return true;
    }

    return false;
}

void FraiseBus::send_to(int dest_id, const char *data, int len) {
    if(len >= 128) return;
    if(dest_id > FRAISE_ID_MAX) return;

    while(com->tx_in_progress()) tight_loop_contents();
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
    receiver->detected(poll_id);
    receiver->received_from(poll_id, rcv_buffer + 1, len);
}

void FraiseBus::service() {
    if(!com->tx_in_progress()) com->set_drive(false);
    while(com->is_readable()) {
        char c = com->getc();
        switch(state) {
            case State::poll: {
                if(rcv_len < 130) rcv_buffer[rcv_len++] = c;
                else {
                    state = State::receive;
                    break;
                }
                if(rcv_len == 1 && c == 0) { // empty answer
                    receiver->detected(poll_id);
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

// --------------------------------------------------------------------------- //

void FraisePoller::set_enable(int id, bool enable) {
    devices[id].enabled = enable;
    devices[id].detected = false;
}

void FraisePoller::service(FraiseBus *bus) {
    if(detected_timeout != at_the_end_of_time) {
        if(time_reached(detected_timeout)) {
            devices[current_id].detected = false;
            detected_timeout = at_the_end_of_time;
        } else return;
    }

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

    if(bus->tx_in_progress()) return;

    if(!time_reached(timeout)) return;
    timeout = make_timeout_time_ms(1);
    for(int i = 0; i <= FRAISE_ID_MAX; i++) {
        current_id = (current_id + 1) % (FRAISE_ID_MAX + 1);
        if(devices[current_id].enabled) {
            bus->poll(current_id);
            detected_timeout = make_timeout_time_ms(10);
            break;
        }
    }
}

void FraisePoller::detected(int id) {
    if((id == current_id) && devices[id].enabled) devices[id].detected = true;
    detected_timeout = at_the_end_of_time;
}

