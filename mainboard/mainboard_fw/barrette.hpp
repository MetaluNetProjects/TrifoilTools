/**
 * interface for the barrette fruit
 */

#pragma once
#include "fraise.hpp"
#include "fraise_bus.hpp"
#include <math.h>

template <unsigned PIXELS> class Barrette {
private:
    uint64_t halls;
    uint32_t framebuffer[PIXELS];
    uint32_t print_framebuffer[PIXELS];
    bool querying = false;
    bool print_leds = true;
    int auto_query_period_ms = 0; // disable if 0
    absolute_time_t query_nexttime = 0;
    absolute_time_t query_timeout = at_the_end_of_time;
    absolute_time_t query_last_time;
    const int QUERY_TIMEOUT_MS = 10;
    unsigned ID;
public:
    Barrette(int id) : ID(id) {}

    unsigned get_id() { return ID;}
    uint64_t get_halls() { return halls;}

    bool is_querying() {
        return querying;
    }

    void service() {
        if(time_reached(query_timeout)) {
            query_timeout = at_the_end_of_time;
            querying = false;
        }

        if((!querying) && (auto_query_period_ms > 0) && time_reached(query_nexttime)) {
            query_nexttime = make_timeout_time_ms(auto_query_period_ms);
            query_halls();
            send_leds();
            //fraise_printf("querying...\n");
        }
    }

    void query_halls() {
        char buffer[4];
        snprintf(buffer, 4, "h%02X", FRAISE_ID);
        fraise_main_bus()->send_to(ID, buffer, 3);
        querying = true;
        query_timeout = make_timeout_time_ms(QUERY_TIMEOUT_MS);
    }

    void set_led(unsigned  numled, uint32_t color) {
        if(numled >= PIXELS) return;
        framebuffer[numled] = color;
    }

    void set_led(unsigned  numled, uint8_t r, uint8_t g, uint8_t b) {
        set_led(numled, (r << 16) + (g << 8) + b);
    }

    bool decode_uint8(const char *& data, uint8_t &len, uint8_t &res) {
        if(len < 2) return false;
        res = gethexbyte(data);
        len -= 2; data += 2;
        return true;
    }

    void encode_uint8(char *buffer, unsigned &index, uint8_t value) {
        uint8_t nibble = (value >> 4) + '0';
        if(nibble > '9') nibble += 'A' - '9' - 1;
        buffer[index++] = nibble;
        nibble = (value & 15) + '0';
        if(nibble > '9') nibble += 'A' - '9' - 1;
        buffer[index++] = nibble;
    }

    void do_print_leds() {
        for(unsigned led = 0; led < PIXELS; led++) {
            if(print_framebuffer[led] != framebuffer[led]) {
                print_framebuffer[led] = framebuffer[led];
                int r = (framebuffer[led] >> 16) & 255;
                int g = (framebuffer[led] >> 8) & 255;
                int b = (framebuffer[led] >> 0) & 255;
                fraise_printf("led %d %d %d %d\n", led, r, g, b);
            }
        }
    }

    void send_leds() {
        char buffer[120];
        static const unsigned leds_per_message = (sizeof(buffer) - 3) / 6;
        unsigned nled = 0;
        while(nled < PIXELS) {
            snprintf(buffer, 4, "P%02X", nled);
            unsigned index = 3;
            for(unsigned i = 0; i < leds_per_message && nled < PIXELS; i++, nled++) {
                encode_uint8(buffer, index, framebuffer[nled] >> 16);
                encode_uint8(buffer, index, framebuffer[nled] >> 8);
                encode_uint8(buffer, index, framebuffer[nled] >> 0);
            }
            buffer[index] = 0;
            fraise_main_bus()->send_to(ID, buffer, index);
            //fraise_printf("%s\n", buffer);
        }
        if(print_leds) do_print_leds();
    }

    void receivechars(const char *data, uint8_t len) {
        char command = data[0];
        len -= 1; data += 1;
        switch(command) {
        case 'H': // set halls
            {
                uint64_t new_halls;
                int ret = sscanf(data, "%016llX", &new_halls);
                if(ret == 1) {
                    halls = new_halls;
                    query_last_time = get_absolute_time();
                    query_timeout = at_the_end_of_time;
                    querying = false;
                    fraise_printf("barrette halls %d %016llX\n", ID, new_halls);
                }
            }
            break;
        case 'q': // auto query period ms
            {
                int ms;
                int ret = sscanf(data, "%04X", &ms);
                
                if(ret == 1) {
                    auto_query_period_ms = ms;
                    query_nexttime = make_timeout_time_ms(auto_query_period_ms);
                    fraise_printf("barrette query ms %d\n", ms);
                }
            }
            break;
        case 't': // print last_query_time_ms
            fraise_printf("barrette last query time ms %d\n", get_last_query_time_ms());
            break;
        case 's': // send leds
            send_leds();
            break;
        case 'P': // pixel led
            {
                if(len < 5) return;
                uint8_t num_led;
                if(!decode_uint8(data, len, num_led)) return;
                while(true) {
                    uint8_t r, g, b;
                    if(num_led >= PIXELS) return;
                    if(!decode_uint8(data, len, r)) return;
                    if(!decode_uint8(data, len, g)) return;
                    if(!decode_uint8(data, len, b)) return;
                    framebuffer[num_led] = (r << 16) + (g << 8) + b;
                    //fraise_printf("led[%d]=%d %d %d\n", num_led, r, g, b);
                    num_led++;
                }
            }
            break;
        }
   }

    void set_auto_query_period_ms(int ms) {
        auto_query_period_ms = ms;
    }

    int get_last_query_time_ms() {
        return absolute_time_diff_us(query_last_time, get_absolute_time()) / 1000;
    }
};

