/**
 * Mainboard firmware
 */

#include "fraise.hpp"
#include "fraise_bus.hpp"
#include "ilotopont3.hpp"
#include "lamp.hpp"
#include "barrette.hpp"
#include "control_logic.hpp"
#include "pico/stdlib.h"

#ifdef PICO_DEFAULT_LED_PIN
const uint LED_PIN = PICO_DEFAULT_LED_PIN;
#endif
int ledPeriod = 250;

const uint PIN_MOT_AL = 0;
const uint PIN_MOT_AH = 1;
const uint PIN_MOT_BL = 2;
const uint PIN_MOT_BH = 3;
const uint PIN_LAMP1 = 4;
const uint PIN_LAMP2 = 5;
const uint PIN_LAMP3 = 6;
const uint PIN_LAMP4 = 7;
const uint PIN_MOT_TEMP = 26;
const uint PIN_MOT_CURRENT = 27;

Ilotopont3 motor(PIN_MOT_AL, PIN_MOT_AH, PIN_MOT_BL, PIN_MOT_BH, PIN_MOT_TEMP, PIN_MOT_CURRENT);

Lamp lamps[4]{PIN_LAMP1, PIN_LAMP2, PIN_LAMP3, PIN_LAMP4};

const int barrette_nb_pixels = 36;
Barrette<barrette_nb_pixels> barrette(10);

TrifoilLogic<barrette_nb_pixels, 4> controller(barrette, motor, lamps);

void setup() {
    motor.set_deadtime_ns(200);
}

void loop(){

#ifdef PICO_DEFAULT_LED_PIN
    static absolute_time_t nextLed;
    static bool led = false;
    if(time_reached(nextLed)) {
        gpio_put(LED_PIN, led = !led);
        nextLed = make_timeout_time_ms(ledPeriod);
    }
#endif
    controller.service();
    motor.service();
    barrette.service();
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
    case 'B': // barrette
        {
            uint8_t id;
            if(!decode_uint8(data, len, id)) return;
            if(barrette.get_id() == id) {
                barrette.receivechars(data, len);
            }
        }
        break;
    case 'M': // Motor
        motor.receivechars(data, len);
        break;
    case 'L': // Logic
        controller.receivechars(data, len);
        break;
    case 'h': // query barrette Hall
        barrette.query_halls();
        break;
    }
}

