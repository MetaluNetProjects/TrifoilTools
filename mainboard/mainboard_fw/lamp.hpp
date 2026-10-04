/**
 * Lamp control interface
 */

#pragma once
#include "fraise.hpp"

class Lamp {
private:
    const int PWM_MAX = clock_get_hz(clk_sys) / 22500; // 22.5kHz
    int pin;
public:
    Lamp(int p) : pin(p) {
        gpio_set_function(pin, GPIO_FUNC_PWM);
        gpio_set_dir(pin, GPIO_OUT);
        uint slice_num = pwm_gpio_to_slice_num(pin);
        pwm_set_wrap(slice_num, PWM_MAX);
        pwm_set_gpio_level(pin, 0);
        pwm_set_enabled(slice_num, true);
    }
    void set(float value){ // value in [0 ; 1.0]
        pwm_set_gpio_level(pin, value * PWM_MAX);
    }
};

