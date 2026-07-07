/**
 * Simple blinking fruit
 */

//#define BOARD pico
#include "fraise.hpp"
#include "fraise_bus.hpp"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
int ledPeriod = 250;

void loop() {
    static absolute_time_t nextLed;
    static bool led = false;

    if(time_reached(nextLed)) {
        gpio_put(LED_PIN, led = !led);
        nextLed = make_timeout_time_ms(ledPeriod);
    }
}

/*void fraise_receivebytes(const char *data, uint8_t len){
    if(data[0] == 1) ledPeriod = (int)data[1] * 10;
    else {
        printf("rcvd ");
        for(int i = 0; i < len; i++) printf("%d ", (uint8_t)data[i]);
        putchar('\n');
    }
}*/


void fraise_receivechars(const char *data, uint8_t len) {
    if(data[0] == 'E') { // Echo
        fraise_printf("E%s\n", data + 1);
    } else if(data[0] == 'P') { // Print
        fraise_main_bus()->queue_message(data + 1, len - 1);
    } else if(data[0] == 'L') { // Led
        ledPeriod = gethexbyte(data + 1) * 10;
    } else if(data[0] == 'V') { // Version
        fraise_printf("V testboard 2 @ %s\n", __DATE__ " " __TIME__);
    } else {
        printf("l rcvd ");
        for(int i = 0; i < len; i++) printf("%d ", data[i]);
        printf("\n");
    }
}

