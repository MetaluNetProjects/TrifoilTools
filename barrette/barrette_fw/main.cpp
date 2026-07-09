/**
 * Simple blinking fruit
 */

#include "fraise.hpp"
#include "hall_shifter.hpp"
#include "pico/stdlib.h"
#include "ws2812.h"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
int ledPeriod = 250;

const uint PIN_HALLSR_SH = 0;
const uint PIN_HALLSR_CK = 1;
const uint PIN_HALLSR_DATA = 2;

const uint PIN_WS8212 = 22;
const bool WS8212_IS_RGBW = false;
const uint WS8212_NUM_PIXELS = 32;

const int num_hall_barrettes = 3;
const int hall_per_barrette = 12;
const int bits_per_barrette = 16;

HallShifter hall_shifter(num_hall_barrettes * bits_per_barrette, PIN_HALLSR_SH, PIN_HALLSR_CK, PIN_HALLSR_DATA);

uint64_t halls;
uint32_t framebuffer[WS8212_NUM_PIXELS];

void pixel_setup() {
    gpio_set_drive_strength(PIN_WS8212, GPIO_DRIVE_STRENGTH_2MA);
    ws2812_setup(PIN_WS8212, WS8212_IS_RGBW);
}

bool pixel_update() {
    static absolute_time_t next_time;
    if(!time_reached(next_time)) return false;
    next_time = make_timeout_time_ms(10);
    ws2812_dma_transfer(framebuffer, WS8212_NUM_PIXELS);
    return true;
}

void setup() {
    pixel_setup();
}

void loop(){
    static absolute_time_t nextLed;
    static bool led = false;

    if(time_reached(nextLed)) {
        gpio_put(LED_PIN, led = !led);
        nextLed = make_timeout_time_ms(ledPeriod);
    }
    if(hall_shifter.service()) {
        if(halls != hall_shifter.get_last()) {
            halls = hall_shifter.get_last();
            fraise_printf("H%016llX\n", halls);
        }
    }
    pixel_update();
}

bool decode_uint8(const char *& data, uint8_t &len, uint8_t &res) {
    if(len < 2) return false;
    res = gethexbyte(data);
    len -= 2; data += 2;
    return true;
}

void fraise_receivechars(const char *data, uint8_t len){
    char command = data[0];
    len -= 1; data += 1;
    switch(command) {
    case 'E': // Echo
        fraise_printf("E%s\n", data);
        break;
    case 'P': // pixel led
        {
            if(len < 5) return;
            uint8_t num_led;
            if(!decode_uint8(data, len, num_led)) return;
            while(true) {
                uint8_t r, g, b;
                if(!decode_uint8(data, len, r)) return;
                if(!decode_uint8(data, len, g)) return;
                if(!decode_uint8(data, len, b)) return;
                framebuffer[num_led] = urgb_u32(r, g, b);
                fraise_printf("led[%d]=%d %d %d\n", num_led, r, g, b);
                num_led++;
            }
        }
        break;
    case 'H': // fake halls
        {
            if(len < 16) return;
            uint64_t new_halls = 0;
            for(int i = 0; i < 8; i++) {
                uint8_t x;
                if(!decode_uint8(data, len, x)) return;
                new_halls += ((uint64_t)x) << ((7 - i) * 8);
            }
            fraise_printf("H%016llX\n", new_halls);
        }
        break;
    }
}

