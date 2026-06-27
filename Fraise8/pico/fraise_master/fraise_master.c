/**
 * Copyright (c) 2024 metalu.net
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "pico/stdlib.h"
#include "hardware/sync.h"

#define FRAISE_DONT_OVERWRITE_PRINTF
#include "fraise_master.h"

#define MAX_FRUITS 64

typedef struct {
    bool detected: 1;
} fruit_vstate_t;

typedef struct {
    bool polled: 1;
    bool detected_sent: 1;
} fruit_state_t;

static volatile fruit_vstate_t fruit_vstates[MAX_FRUITS];
static fruit_state_t fruit_states[MAX_FRUITS];

static inline void set_polled(uint8_t id, bool v) {
    fruit_states[id].polled = v;
}
static inline bool is_polled(uint8_t id) {
    return fruit_states[id].polled;
}

static inline void set_detected(uint8_t id, bool v) {
    fruit_vstates[id].detected = v;
}
static inline bool is_detected(uint8_t id) {
    return fruit_vstates[id].detected;
}

static inline void set_detected_sent(uint8_t id, bool v) {
    fruit_states[id].detected_sent = v;
}
static inline bool is_detected_sent(uint8_t id) {
    return fruit_states[id].detected_sent;
}

static bool is_initialized = false;

static uint8_t polled_fruit = 1;
static uint8_t destination_fruit = 0;

typedef enum {
    FMS_POLL,
    FMS_RECEIVE,
    FMS_SEND,
    FMS_WAITACK,
    FMS_BOOTLOAD
} fraise_master_state_t;

static fraise_master_state_t state = FMS_POLL;

static inline bool is_bootloading() {
    return state == FMS_BOOTLOAD;
}

void fraise_setup() {
    if(is_initialized) return;

    fraise_uart_init();
    state = FMS_POLL;
    is_initialized = true;
}

void fraise_get_pins(int *rxpin, int *txpin, int *drvpin, int *drvlevel)
{
    *rxpin = FRAISE_RX_PIN;
    *txpin = FRAISE_TX_PIN;
    *drvpin = FRAISE_DRV_PIN;
    *drvlevel = FRAISE_DRV_LEVEL;
}

void fraise_unsetup() {
    if(!is_initialized) return;
    fraise_master_reset_polls();
    fraise_uart_set_drive(false);
    is_initialized = false;
}

void fraise_master_reset() {
    fraise_uart_set_drive(false);
    state = FMS_POLL;
}

// -----------------------------

void fraise_master_bootload_start(const char *fruit_id_string) {
    if(strlen(fruit_id_string) != 2) {
        printf("e wrong fruit id %s", fruit_id_string);
        return;
    }
    state = FMS_BOOTLOAD;
    char buffer[32];
    sprintf(buffer, "F%s", fruit_id_string);
    fraise_master_sendchars_broadcast(buffer);
}

void fraise_master_bootload_stop() {
    state = FMS_POLL;
}

bool fraise_master_is_bootloading() {
    return is_bootloading();
}

#define UART_PUTC_SUM(c, checksum) do{ char cc = c; fraise_uart_putc(cc); checksum += cc; } while(0)
#define UART_PUT_CHKSUM(checksum) do{ fraise_uart_putc((uint8_t)(128 - (checksum & 127))); } while(0)

void fraise_master_bootload_send(const char *buf, int len) {
    if(!is_bootloading()) return;
    if(len >= 128) return;
    uint8_t checksum;
    const char *p = buf;

    fraise_uart_set_drive(true);
    UART_PUTC_SUM(len, checksum);
    while(len--) {
        UART_PUTC_SUM(*p, checksum);
        p++;
    }
    UART_PUT_CHKSUM(checksum);
    fraise_uart_set_drive(false);
}

// -----------------------------

void fraise_master_set_poll(uint8_t id, bool poll) {
    if(id == 0) {
        if(poll) printf("sC00\n");
        else printf("sc00\n");
    } else if(id < MAX_FRUITS) {
        //uint32_t status = save_and_disable_interrupts();
        set_polled(id, poll);
        set_detected(id, false);
        //restore_interrupts(status);
        //printf("l polled(%d):%d\n", id, is_polled(id));
    }
}

void fraise_master_reset_polls() {
    //uint32_t status = save_and_disable_interrupts();
    for(int id = 0; id < MAX_FRUITS ; id++) {
        set_polled(id, false);
        set_detected(id, false);
        set_detected_sent(id, false);
    }
    //restore_interrupts(status);
}

// -----------------------------

void fraise_master_sendbytes_raw(uint8_t id, const char *data, uint8_t len, bool isChar) {
    if(id >= MAX_FRUITS) return;
    if(len >= (127 + isChar)) return;
    uint8_t checksum = 0;
    const char *p = data;

    fraise_uart_set_drive(true);
    UART_PUTC_SUM(id + 128, checksum);
    UART_PUTC_SUM(len, checksum);
    if(! isChar) UART_PUTC_SUM(0, checksum); // is raw bytes, prefix with null byte
    // TODO: convert to 7bit if !isChar
    while(len--) {
        UART_PUTC_SUM(*p, checksum);
        p++;
    }
    UART_PUT_CHKSUM(checksum);
    fraise_uart_set_drive(false);
}

void fraise_master_sendbytes(uint8_t id, const char *data, uint8_t len) {
    if(id == 0) {
        fraise_init_get_buffer(data, len);
        fraise_receivebytes(data, len);
        return;
    }
    fraise_master_sendbytes_raw(id, data, len, false);
}

void fraise_master_sendchars(uint8_t id, const char *data) {
    int len = strlen(data);
    if(id == 0) {
        fraise_receivechars(data, len);
        return;
    }
    fraise_master_sendbytes_raw(id, data, len, true);
}

void fraise_master_sendbytes_broadcast(const char *data, uint8_t len) {
    fraise_init_get_buffer(data, len);
    fraise_receivebytes_broadcast(data, len);
    fraise_master_sendbytes_raw(0, data, len, false);
}

void fraise_master_sendchars_broadcast(const char *data) {
    int len = strlen(data);
    fraise_receivechars_broadcast(data, len);
    fraise_master_sendbytes_raw(0, data, len, true);
}

// -----------------------------

void fraise_master_service() {
    // Process incoming message
    char buf[256];
    int len;
    /*while((len = rxbuf_read_init())) {
        bool isChar = len > 127;
        len = (len & 63) - 2;
        uint8_t id = rxbuf_read_getc();
        for(int i = 0; i < len; i++) buf[i] = rxbuf_read_getc();
        if(isChar) {
            printf("%02X", id | 128);
            for(int i = 0; i < len; i++) putchar(buf[i]);
            putchar('\n');
            buf[len] = 0; // end string
            fraise_master_receivechars(id, buf, len);
        }
        else {
            printf("%02X", id);
            for(int i = 0; i < len; i++) printf("%02X", buf[i]);
            putchar('\n');
            fraise_master_receivebytes(id, buf, len);
        }
        rxbuf_read_finish();
    }*/

    // TODO: pace 'detected changes' updates
    for(int i = 1; i < MAX_FRUITS; i++) {
        if(is_detected_sent(i) != is_detected(i)) {
            bool d = is_detected(i);
            if(d) {
                set_detected_sent(i, true);
                fraise_master_fruit_detected(i, true);
                printf("sC%02X\n", i);
            }
            else {
                set_detected_sent(i, false);
                fraise_master_fruit_detected(i, false);
                printf("sc%02X\n", i);
            }
        }
    }
}

void fraise_master_print_stats() {
    //printf("l state:%d ferr_count:%d\n", state, fraise_master_ferr_count);
    printf("l state:%d\n", state);
    for(int i = 1; i < 3; i++) {
        printf("l fruit_%d polled=%d detected=%d detected_sent=%d\n",
               i, is_polled(i), is_detected(i), is_detected_sent(i));
    }
}

void fraise_print_status() {
    //printf("l fr psol %d %d %d %d\n", PIO_NUM(pio), sm, pgm_offset, fraise_program.length); // pio sm offset length
}

// ------------------------------
// virtual fruit fraise_put* emulation

// Send a text message (must be a null-terminated string)
bool fraise_puts(const char* msg) {
    printf("80%s\n", msg);
    fraise_master_receivechars(0, msg, strlen(msg));
    return true;
}

// Send a raw bytes message
bool fraise_putbytes(const char* data, uint8_t len) {
    printf("00");
    for(int i = 0; i < len ; i++) printf("%02X", data[i]);
    putchar('\n');
    fraise_master_receivebytes(0, data, len);
    return true;
}

// ------------------------------
// virtual fruit stdout emulation

void fraise_putchar(char c) {
    static char line[512];
    static int count = 0;
    if(c == '\n') {
        line[count] = 0;
        printf("80%s\n", line);
        fraise_master_receivechars(0, line, strlen(line));
        count = 0;
        return;
    }
    if(count < sizeof(line) - 1) line[count++] = c;
}

void fraise_printf(const char* fmt, ...) {
    va_list args;
    char buf[64];
    char *p = buf;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    while(*p) fraise_putchar(*p++);
}

