/**
 * Simple blinking fruit
 */

#include "fraise.hpp"
#include "fraise_bus.hpp"
#include "hall_shifter.hpp"
#include "pico/stdlib.h"
#include "ws2812.h"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
int ledPeriod = 250;

const uint PIN_HALLSR_SH = 0;
const uint PIN_HALLSR_CK = 1;
const uint PIN_HALLSR_DATA = 2;

//const uint PIN_WS2812 = 22;
const uint PIN_WS2812 = 19;
const bool WS2812_IS_RGBW = false;
const uint WS2812_NUM_PIXELS = 36;

const int num_hall_barrettes = 3;
const int halls_per_barrette = 12;
const int bits_per_barrette = 16;

HallShifter hall_shifter(num_hall_barrettes * bits_per_barrette, PIN_HALLSR_SH, PIN_HALLSR_CK, PIN_HALLSR_DATA);
bool shifter_enable = true;

uint64_t halls;
uint32_t framebuffer[WS2812_NUM_PIXELS];

void pixel_setup() {
    gpio_set_drive_strength(PIN_WS2812, GPIO_DRIVE_STRENGTH_2MA);
    ws2812_setup(PIN_WS2812, WS2812_IS_RGBW);
}

bool pixel_update() {
    static absolute_time_t next_time;
    if(!time_reached(next_time)) return false;
    next_time = make_timeout_time_ms(10);
    ws2812_dma_transfer(framebuffer, WS2812_NUM_PIXELS);
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
    if(shifter_enable && hall_shifter.service()) {
        uint64_t shifter_last = hall_shifter.get_last();
        uint64_t new_halls = 0;
        for(int barrette = 0; barrette < num_hall_barrettes; barrette++) {
            if(barrette != 0) {
                new_halls <<= halls_per_barrette;
            }
            new_halls += shifter_last & ((1 << halls_per_barrette) - 1);
            shifter_last >>= bits_per_barrette;
        }
        if(halls != new_halls) {
            halls = new_halls;
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
                if(num_led >= WS2812_NUM_PIXELS) return;
                if(!decode_uint8(data, len, r)) return;
                if(!decode_uint8(data, len, g)) return;
                if(!decode_uint8(data, len, b)) return;
                framebuffer[num_led] = urgb_u32(r, g, b);
                //fraise_printf("led[%d]=%d %d %d\n", num_led, r, g, b);
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
            halls = new_halls;
            fraise_printf("H%016llX\n", new_halls);
        }
        break;
    case 'h': // send halls to other id
        {
            uint8_t dest_id;
            if(!decode_uint8(data, len, dest_id)) return;
            char buffer[128];
            int buflen = 0;
            buflen = sprintf(buffer, "B%02XH%016llX\n", FRAISE_ID, halls);
            fraise_main_bus()->send_to(dest_id, buffer, buflen);
        }
        break;
    case 'S': // shifter enable
        shifter_enable = (*data != '0');
        break;
    }
}

