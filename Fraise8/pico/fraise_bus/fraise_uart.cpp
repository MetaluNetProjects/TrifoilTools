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
    /*if(!drive) {
        while(tx_in_progress()) tight_loop_contents();
    }*/
    gpio_put(drive_pin, drive_level ? drive : !drive);
}

static int64_t tx_end_callback(alarm_id_t id, void *user_data) {
    FraiseUart *uart = (FraiseUart *)user_data;
    uart->set_drive(false);
    return 0;
}

void FraiseUart::send(const char *data, uint8_t len) {
    set_drive(true);
    add_alarm_in_us(((int)len) * ((10 * 1000000) /FRAISE_UART_BAUDRATE), tx_end_callback, this, true);
    for(int i = 0; i < len; i++) putc(data[i]);
    //while(tx_in_progress()) tight_loop_contents();
    //set_drive(false);
}

//void FraiseUart::set_irq_handler(irq_handler_t handler) {}
//void FraiseUart::set_irqs_enabled (bool rx_has_data, bool tx_needs_data) {}


