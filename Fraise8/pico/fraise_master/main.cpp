/**
 * Copyright (c) 2026 metalu.net
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAISE_DONT_OVERWRITE_PRINTF
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "fraise_eeprom.h"
//#include "fraise_master.h"
#include "fraise.h"
#include "fraise_bus.hpp"
#include "pico/multicore.h"

#if PICO_RP2040
#include "RP2040.h"
#else
#include "RP2350.h"
#endif

FraiseBus bus(
    new FraiseUart(UART_INSTANCE(FRAISE_UART_NUM), FRAISE_TX_PIN, FRAISE_RX_PIN, FRAISE_DRV_PIN, FRAISE_DRV_LEVEL), 
    FRAISE_ID
);

char lineBuf[1024];
uint8_t lineLen;

static void fraise_disable_interrupts(void)
{
    SysTick->CTRL &= ~1;

    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICPR[0] = 0xFFFFFFFF;
}

static void reset_peripherals(void)
{
    reset_block(~(
        RESETS_RESET_IO_QSPI_BITS |
        RESETS_RESET_PADS_QSPI_BITS |
        RESETS_RESET_SYSCFG_BITS |
        RESETS_RESET_PLL_SYS_BITS
    ));
}

void reboot() {
    sleep_ms(50); // wait for the host to disconnect the USB device
    fraise_disable_interrupts();
    multicore_reset_core1();
    reset_peripherals();
    watchdog_reboot(0, 0, 0);
    while (1) {
        tight_loop_contents();
    }
}

#define startsWith(str, prefix) (!(strncmp((const char *)(str), (const char *)(prefix), strlen(prefix))))

void processLine() {
    if(lineBuf[0] == '#' && lineBuf[1] == 'V') {
        printf("sV UsbFraise PicoPied v0.2\n");
        return;
    }

    if(bus.process_command(lineBuf)) return;

    if(startsWith(lineBuf, "waitack")) printf("ack\n");
    else if(startsWith(lineBuf, "reboot")) {
        sleep_ms(100); // wait for the host to disconnect the USB device
        reboot();
    }
    else if(startsWith(lineBuf, "whoami")) {
        printf("swhoami fraise_master\n");
    }
}

void stdioTask()
{
    int c;
    while((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if(c == '\n') {
            lineBuf[lineLen] = 0;
            processLine();
            lineLen = 0;
        }
        else if(c == '&') { // inner line break
            char tmp = lineBuf[lineLen];
            lineBuf[lineLen] = 0;
            processLine();
            lineLen = 0;
            lineBuf[lineLen] = tmp;
        }
        else lineBuf[lineLen++] = c;
    }
}

int main() {
    stdio_init_all();
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
    eeprom_setup();
    setup();
    while(true) {
        stdioTask();
        bus.service();
        loop();
    }
}

__attribute__((weak)) void setup() {}
__attribute__((weak)) void loop() {}
#define STRINGIFY(x) #x
#ifdef FRAISE_MASTER_DEBUG
#define dummy_callback(f) __attribute__((weak)) void f(const char *data, uint8_t len){ printf("dummy " STRINGIFY(f) "()\n");}
#else
#define dummy_callback(f) __attribute__((weak)) void f(const char *data, uint8_t len){}
#endif

dummy_callback(fraise_receivebytes);
dummy_callback(fraise_receivechars);
dummy_callback(fraise_receivebytes_broadcast);
dummy_callback(fraise_receivechars_broadcast);

// ------------------------------
// virtual fruit stdout emulation

void fraise_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("80");
    vprintf(fmt, args);
}

/*__attribute__((weak)) void fraise_master_receivebytes(uint8_t fruit_id, const char *data, uint8_t len) {}
__attribute__((weak)) void fraise_master_receivechars(uint8_t fruit_id, const char *data, uint8_t len) {}
__attribute__((weak)) void fraise_master_fruit_detected(uint8_t fruit_id, bool detected) {}*/


