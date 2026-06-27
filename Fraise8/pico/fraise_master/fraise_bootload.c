/**
 * Copyright (c) 2024 metalu.net
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#define FRAISE_DONT_OVERWRITE_PRINTF
#include "fraise.h"
#include "fraise_master.h"
#include "fraise_master_buffers.h"

#define MAX_NB_RETRIES 10

bool usePicoBootloader = false;
bool waitAck = false;

uint8_t gethexbyte(const char *buf)
{
    uint8_t cl, ch;
    ch = buf[0] - '0';
    if(ch > 9) ch += '9' - 'A' + 1;
    cl = buf[1] - '0';
    if(cl > 9) cl += '9' - 'A' + 1;
    return (ch << 4) + cl;
}

void fraise_master_bootload_getline(const char *lineBuf, int lineLen) {
    if(lineBuf[0] == '!' && lineBuf[1] == 'F') {
        fraise_master_sendchars_broadcast(lineBuf + 1);//, lineLen - 1);
        waitAck = false;
        sleep_ms(1);
    } else fraise_master_bootload_send(lineBuf, strlen(lineBuf));
}

void fraise_bootloader_use_pico(bool useit) {
    usePicoBootloader = false; //useit;
}

void fraise_master_bootload_service() {
    while(fraise_uart_is_readable()) printf("b%c\n", fraise_uart_getc());
}

