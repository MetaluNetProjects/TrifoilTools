/**
 * Copyright (c) 2023 metalu.net
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "pico/stdlib.h"
//#include "pico/async_context_threadsafe_background.h"
//#include "pico/async_context_poll.h"
//#include "hardware/pio.h"
#if PICO_RP2040
#include "RP2040.h"
#else
#include "RP2350.h"
#endif
#include "hardware/resets.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"

#include "fraise.hpp"
#include "fraise_bus.hpp"
#include "fraise_eeprom.h"

FraiseBus bus(
    new FraiseUart(FRAISE_TX_PIN, FRAISE_RX_PIN, FRAISE_DRV_PIN, FRAISE_DRV_LEVEL), 
    FRAISE_ID
);

FraiseBus *fraise_main_bus() {
    return &bus;
}

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
    hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
    watchdog_reboot(0, 0, 0);

    while (1) {
        tight_loop_contents();
    }
}

extern int __fraise_bootloader_start__;

void switch_to_bootloader()
{
    //fraise_unsetup();
    fraise_disable_interrupts();
    reset_peripherals();
    const uint32_t vtor = (uint32_t)&__fraise_bootloader_start__ /*XIP_BASE + (64 * 1024)*/;
    // Derived from the Leaf Labs Cortex-M3 bootloader.
    // Copyright (c) 2010 LeafLabs LLC.
    // Modified 2021 Brian Starkey <stark3y@gmail.com>
    // Originally under The MIT License
    uint32_t reset_vector = *(volatile uint32_t *)(vtor + 0x04);

    SCB->VTOR = (volatile uint32_t)(vtor);

    asm volatile("msr msp, %0"::"g" (*(volatile uint32_t *)vtor));
    asm volatile("bx %0"::"r" (reset_vector));

    while(1);
}

void switch_to_bootloader_if_name_matches(const char *data, uint8_t len)
{
    if(gethexbyte(data) != FRAISE_ID) {
    //if(strncmp(data, eeprom_get_name(), len)) {
#ifdef FRAISE_DEVICE_DEBUG
        printf("l name mismatch!\n");
#endif
        return; // returnif the name doesn't match
    }
#ifdef FRAISE_DEVICE_DEBUG
    printf("l name match, rebooting\n");
#endif
    //switch_to_bootloader();
    reboot();
}

struct FraiseReceiverMaster: public FraiseReceiver {
    virtual void sent_to(int dest_id, const char *data, int len) override {
        printf("l sent_to %d: ", dest_id);
        for(int i = 0; i < len; i++) printf("%d ", data[i]);
        printf("\n");
        if(dest_id == 0) { // broadcast
            char c = data[0];
            if(c == 'B') fraise_receivechars_broadcast(data + 1, len - 1);
            else if(c == 'F') switch_to_bootloader_if_name_matches(data + 1, len - 1);
        }
    }
    //virtual void received_from(int src_id, const char *data, int len) override {}
    //virtual void received(const char *data, int len) override {}
} receiver;

void fraise_putchar(char c) {
    static char line[128];
    static unsigned int count = 0;
    if(c == '\n') {
        line[count] = 0;
        bus.queue_message(line, count);
        count = 0;
        return;
    }
    if(count < sizeof(line)) line[count++] = c;
}

void fraise_printf(const char* fmt, ...) {
    va_list args;
    char buf[256];
    char *p = buf;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    while(*p) fraise_putchar(*p++);
}

int main() {
    stdio_init_all();
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
    bus.set_receiver(&receiver);
    eeprom_setup();
    setup();
    while(true) {
        bus.service();
        loop();
    }
}

__attribute__((weak)) void setup(){}
__attribute__((weak)) void loop(){}
#define STRINGIFY(x) #x
#ifdef FRAISE_DEVICE_DEBUG
#define dummy_callback(f) __attribute__((weak)) void f(const char *data, uint8_t len){ printf("dummy " STRINGIFY(f) "()\n");}
#else
#define dummy_callback(f) __attribute__((weak)) void f(const char *data, uint8_t len){}
#endif

dummy_callback(fraise_receivebytes);
dummy_callback(fraise_receivechars);
dummy_callback(fraise_receivebytes_broadcast);
dummy_callback(fraise_receivechars_broadcast);

