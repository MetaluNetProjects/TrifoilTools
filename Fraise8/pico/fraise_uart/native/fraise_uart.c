#include "pico/stdlib.h"
#include "fraise_uart.h"
#include <hardware/uart.h>

#if (FRAISE_TX_PIN == 0) || (FRAISE_TX_PIN == 12) || (FRAISE_TX_PIN == 16) || (FRAISE_TX_PIN == 28)
#define FRAISE_UART uart0
#else
#define FRAISE_UART uart1
#endif

void fraise_uart_init() {
    uart_init(FRAISE_UART, FRAISE_UART_BAUDRATE);
    gpio_set_function(FRAISE_TX_PIN, UART_FUNCSEL_NUM(FRAISE_UART, UART_TX_PIN));
    gpio_set_function(FRAISE_RX_PIN, UART_FUNCSEL_NUM(FRAISE_UART, UART_RX_PIN));
    gpio_init(FRAISE_DRV_PIN);
    gpio_set_dir(FRAISE_DRV_PIN, GPIO_OUT);
    gpio_put(FRAISE_DRV_PIN, !FRAISE_DRV_LEVEL);
}

char fraise_uart_getc() {
    return uart_getc(FRAISE_UART);
}

void fraise_uart_putc (char c) {
    uart_putc_raw(FRAISE_UART, c);
}

bool fraise_uart_is_readable() {
    return uart_is_readable(FRAISE_UART);
}

bool fraise_uart_is_writable() {
    return uart_is_writable(FRAISE_UART);
}

void fraise_uart_tx_wait_blocking() {
    uart_tx_wait_blocking(FRAISE_UART);
}

void fraise_uart_set_drive(bool drive) {
    if(!drive) fraise_uart_tx_wait_blocking();
    gpio_put(FRAISE_DRV_PIN, FRAISE_DRV_LEVEL ? drive : !drive);
}

