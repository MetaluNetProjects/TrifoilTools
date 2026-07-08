/**
 * Simple blinking fruit
 */

#include "fraise.hpp"
#include "hall_shifter.hpp"
#include "pico/stdlib.h"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
int ledPeriod = 250;

const uint PIN_HALLSR_SH = 0;
const uint PIN_HALLSR_CK = 1;
const uint PIN_HALLSR_DATA = 2;

const uint PIN_WS8212 = 22;

const int num_hall_barrettes = 3;
const int hall_per_barrette = 12;
const int bits_per_barrette = 16;

HallShifter hall_shifter(num_hall_barrettes * bits_per_barrette, PIN_HALLSR_SH, PIN_HALLSR_CK, PIN_HALLSR_DATA);

uint64_t halls;

void loop(){
    static absolute_time_t nextLed;
    static bool led = false;

    if(time_reached(nextLed)) {
        gpio_put(LED_PIN, led = !led);
        nextLed = make_timeout_time_ms(ledPeriod);
    }
    if(hall_shifter.service()) {
        if(halls != hall_shifter.get_last()) {
            halls != hall_shifter.get_last();
            fraise_printf("H%016llX\n", halls);
        }
    }
}

void fraise_receivechars(const char *data, uint8_t len){
    if(data[0] == 'E') { // Echo
        printf("E%s\n", data + 1);
    }
}

