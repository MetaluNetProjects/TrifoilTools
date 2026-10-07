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
#include "pico/sync.h"
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
    gpio_set_function(txpin, UART_FUNCSEL_NUM(uart, FRAISE_TX_PIN));
    gpio_set_function(rxpin, UART_FUNCSEL_NUM(uart, FRAISE_RX_PIN));
    gpio_init(drive_pin);
    gpio_set_dir(drive_pin, GPIO_OUT);
    gpio_put(drive_pin, !drive_level);
    gpio_pull_up(rxpin);
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
    return (uart_get_hw(uart)->fr & UART_UARTFR_BUSY_BITS) || (drive_stop_alarm != 0);
}

void FraiseUart::set_drive(bool drive) {
    /*if(!drive) {
        while(tx_in_progress()) tight_loop_contents();
    }*/
    if(drive_stop_alarm) cancel_alarm(drive_stop_alarm);
    drive_stop_alarm = 0;
    gpio_put(drive_pin, drive_level ? drive : !drive);
}

int64_t FraiseUart::tx_end_callback(alarm_id_t id, void *user_data) {
    FraiseUart *uart = (FraiseUart *)user_data;
    while(uart_get_hw(uart->uart)->fr & UART_UARTFR_BUSY_BITS) {}
    uart->drive_stop_alarm = 0;
    uart->set_drive(false);
    return 0;
}

void FraiseUart::send(const char *data, uint8_t len) {
    int drive_us = ((int)len) * ((10 * 1000000) / FRAISE_UART_BAUDRATE) + 2;
    while(!is_writable()) {}
    uint32_t status = save_and_disable_interrupts();
    set_drive(true);
    drive_stop_alarm = add_alarm_in_us(drive_us, tx_end_callback, this, true);
    putc(data[0]);
    restore_interrupts_from_disabled(status);
    for(int i = 1; i < len; i++) putc(data[i]);
    //if(len > 2) printf("FraiseUart::send %d %dus\n", len, drive_us);
    //while(tx_in_progress()) tight_loop_contents();
    //set_drive(false);
}

//void FraiseUart::set_irq_handler(irq_handler_t handler) {}
//void FraiseUart::set_irqs_enabled (bool rx_has_data, bool tx_needs_data) {}


