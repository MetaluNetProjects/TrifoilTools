/**
 * Copyright (c) 2026 metalu.net
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include <stdio.h>
#define FRAISE_DONT_OVERWRITE_PRINTF
#include "fraise.h"
#include "fraise_bus.hpp"
#include <hardware/uart.h>
#include <cstring>

#define FRAISE_UART_BAUDRATE 250000

FraiseUart::FraiseUart(uart_inst_t *uart, int txpin, int rxpin, int drvpin, bool drvlevel):
    uart(uart), drive_pin(drvpin), drive_level(drvlevel)
{
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
    gpio_put(drive_pin, drive_level ? drive : !drive);
}

void FraiseUart::send(const char *data, uint8_t len) {
    set_drive(true);
    for(int i = 0; i < len; i++) putc(data[i]);
    while(tx_in_progress()) tight_loop_contents();
    set_drive(false);
}

void FraiseUart::set_irq_handler(irq_handler_t handler) {}
void FraiseUart::set_irqs_enabled (bool rx_has_data, bool tx_needs_data) {}

// --------------------------------------------------------------------------- //

FraiseBus::FraiseBus(FraiseCom *com, int id):
    com(com), id(id)
{
}

#define ishex(x) ((x >= '0'&& x <='9') || (x >= 'A' && x <= 'F'))
static uint8_t gethexbyte(const char *buf)
{
    uint8_t cl, ch;
    ch = buf[0] - '0';
    if(ch > 9) ch += '9' - 'A' + 1;
    cl = buf[1] - '0';
    if(cl > 9) cl += '9' - 'A' + 1;
    return (ch << 4) + cl;
}

bool FraiseBus::process_command(char *data) {
    int len = strlen(data);
    if(data[0] == '#') { // system
        switch(data[1]) {
        case 'R':
            printf("sID%02X\n", id);
            //fraise_master_bootload_stop();
            break;
        case 'E':
            puts((const char*)(data + 2));
            break;
        case 'S':
            if(gethexbyte(data + 2) == 0) printf("sC00\n");
            //fraise_master_set_poll(gethexbyte(data + 2), true);
            break;
        case 'C':
            if(gethexbyte(data + 2) == 0) printf("sc00\n");
            //fraise_master_set_poll(gethexbyte(data + 2), false);
            break;
        case 'i':
            //fraise_master_reset_polls();
            break;
        case 'F':
            //fraise_master_bootload_stop();
            break;
        }
        return true;
    }
    //else if(fraise_master_is_bootloading()) fraise_master_bootload_getline(data, len);
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
            break;
        case 'F':
            fraise_master_bootload_start(data + 2);
            break;*/
        }
        return true;
    }
    else if(ishex(data[0]) && ishex(data[1])) { // normal output to fruit
        uint8_t dest_id = gethexbyte(data) - 128;
        if(dest_id == 0) {
            fraise_receivechars(data + 2, len - 2);
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

void FraiseBus::poll(int poll_id) {
    if(poll_id > FRAISE_ID_MAX || poll_id <= 0) return;
    char buffer[2] = {(char)(poll_id + 128), 0};
    com->send(buffer, 2);
}

bool FraiseBus::queue_for_polling(const char *data, int len) {
    return true;
}

void FraiseBus::service() {
}

void FraiseBus::received_from(int src_id, const char *data, int len) {}
void FraiseBus::received(const char *data, int len) {}


