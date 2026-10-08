/**
 * Simple blinking fruit
 */

//#define BOARD pico
#include "fraise.hpp"
#include "fraise_bus.hpp"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
int ledPeriod = 250;

extern FraisePoller poller;

static struct FraiseReceiverMaster: public FraiseReceiver {
    /*virtual void sent_to(int dest_id, const char *data, int len) {
        fraise_printf("sent %d %.*s\n", dest_id, len, data);
    }*/
    virtual void received_from(int src_id, const char *data, int len) override {
        printf("%02X%s\n", src_id + 128, data);
    }
    //virtual void received(const char *data, int len);
    virtual void detected(int src_id, bool is_detected) override {
        poller.detected(src_id, is_detected);
    }
} receiver;

void setup() {
    fraise_main_bus()->set_receiver(&receiver);
}

void loop(){
    static absolute_time_t nextLed;
    static bool led = false;

    if(time_reached(nextLed)) {
        gpio_put(LED_PIN, led = !led);
        nextLed = make_timeout_time_ms(ledPeriod);
    }
}

//FraisePoller &poller = *fraise_master_get_poller();

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
    case 'p': // poll pause
        poller.poll_pause_ms(gethexbyte(data + 1));
        break;
    }
}

