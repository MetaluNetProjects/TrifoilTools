#ifndef _FRAISE_UART_H
#define _FRAISE_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#define FRAISE_UART_BAUDRATE 250000

void fraise_uart_init();

char fraise_uart_getc(); 
void fraise_uart_putc (char c);
bool fraise_uart_is_readable();
bool fraise_uart_is_writable();
void fraise_uart_tx_wait_blocking();
void fraise_uart_set_drive(bool drive);

#ifdef __cplusplus
}
#endif

#endif // ifndef _FRAISE_UART_H
