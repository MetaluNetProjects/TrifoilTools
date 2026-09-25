/**
 * interface for the barrette fruit
 */

#pragma once
#include "fraise.hpp"
#include "fraise_bus.hpp"
#include <math.h>

template <unsigned ID, unsigned PIXELS> class Barrette {
private:
    uint64_t halls;
    uint32_t framebuffer[PIXELS];
    bool querying = false;
    int auto_query_period_ms = 0; // disable if 0
    absolute_time_t query_nexttime = 0;
    absolute_time_t query_timeout = at_the_end_of_time;
    absolute_time_t query_last_time;
    const int QUERY_TIMEOUT_MS = 10;
public:
    Barrette() {}

    unsigned get_id() { return ID;}

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

    void receivechars(const char *data, uint8_t len) {
        char command = data[0];
        len -= 1; data += 1;
        switch(command) {
        case 'H': // halls
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
        }
    }

    void set_auto_query_period_ms(int ms) {
        auto_query_period_ms = ms;
    }

    int get_last_query_time_ms() {
        return absolute_time_diff_us(query_last_time, get_absolute_time()) / 1000;
    }
};

