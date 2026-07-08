/**
 * Simple blinking fruit
 */

#pragma once
#include "fraise.hpp"

class HallShifter {
private:
    uint64_t tmp_register = 0;
    uint64_t last_register = 0;
    int pulse_count = -1;
    int total_count;
    int pin_sh;
    int pin_ck;
    int pin_data;
    absolute_time_t next_bit_time;
    const int bitrate = 10000;
public:
    HallShifter(int count, int sh, int ck, int data): total_count(count), pin_sh(sh), pin_ck(ck), pin_data(data) {
        gpio_init(pin_sh);
        gpio_set_dir(pin_sh, GPIO_OUT);
        gpio_put(pin_sh, false);
        gpio_init(pin_ck);
        gpio_set_dir(pin_ck, GPIO_OUT);
        gpio_put(pin_ck, false);
        gpio_init(pin_data);
        gpio_set_dir(pin_data, GPIO_IN);
        gpio_pull_up(pin_data);
    }
    bool service() {
        if(!time_reached(next_bit_time)) return false;
        next_bit_time = make_timeout_time_us(1e6 / bitrate);
        if(pulse_count == -1) {
            gpio_put(pin_sh, true);
        } else {
            if((pulse_count % 2) == 0) {
                tmp_register = (tmp_register << 1) | gpio_get(pin_data);
                gpio_put(pin_ck, true);
            } else {
                gpio_put(pin_ck, false);
            }
        }
        pulse_count += 1;
        if(pulse_count == (total_count * 2)) {
            gpio_put(pin_sh, false);
            pulse_count = -1;
            last_register = tmp_register;
            tmp_register = 0;
            return true;
        }
        return false;
    }
    uint64_t get_last() {
        return last_register;
    }
};

