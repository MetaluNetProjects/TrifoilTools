/**
 * Simple blinking fruit
 */

#include "fraise.hpp"
#include "ilotopont3.hpp"
#include "pico/stdlib.h"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
int ledPeriod = 250;

const uint PIN_MOT_AL = 0;
const uint PIN_MOT_AH = 1;
const uint PIN_MOT_BL = 2;
const uint PIN_MOT_BH = 3;
const uint PIN_MOT_TEMP = 26;
const uint PIN_MOT_CURRENT = 27;

uint64_t halls;

Ilotopont3 ilotopont(PIN_MOT_AL, PIN_MOT_AH, PIN_MOT_BL, PIN_MOT_BH, PIN_MOT_TEMP, PIN_MOT_CURRENT);
MotorControl &motor = ilotopont;

void setup() {
}

void loop(){
    static absolute_time_t nextLed;
    static bool led = false;

    if(time_reached(nextLed)) {
        gpio_put(LED_PIN, led = !led);
        nextLed = make_timeout_time_ms(ledPeriod);
    }
    motor.service();
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
    case 'H': // barrette Hall
        {
            int src_id;
            uint64_t new_halls;
            int ret = sscanf(data, "%02X%016llX", &src_id, &new_halls);
            if(ret == 2 && src_id == 10) {
                halls = new_halls;
            }
        }
        break;
    case 'M': // Motor
        motor.receivechars(data, len);
        break;
    }
}

