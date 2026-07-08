/**
 * Simple blinking fruit
 */

//#define BOARD pico
#include "fraise.hpp"
#include "fraise_bus.hpp"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
int ledPeriod = 250;

void loop(){
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

void fraise_receivechars(const char *data, uint8_t len){
    char command = data[0];
    switch(command) {
    case 'E':
        fraise_printf("E%s\n", data + 1);
        break;
    case 'P':
        fraise_main_bus()->poll(gethexbyte(data + 1));
        break;
    case 'S':
        fraise_printf("state %s\n", fraise_main_bus()->get_state_name());
        break;
    }
}

