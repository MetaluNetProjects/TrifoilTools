/**
 * Ilotopont3 board driver
 */

#pragma once
#include "motorcontrol.hpp"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "fraise.hpp"
#include <math.h>
#include <algorithm>

class Ilotopont3 : public MotorControl {
private:
    const int PWM_MAX = clock_get_hz(clk_sys) / (22500 * 2); // 22.5kHz ; half the value because of phase correct
    int pin_AL, pin_AH, pin_BL, pin_BH, pin_temp, pin_current;

    float temperature_C = 0;
    float current_mA = 0;

    const int update_period_ms = 2;
    absolute_time_t update_timeout;

    const float adc_filter_hz = 4.0;
    const float adc_filter = adc_filter_hz * 6.28 * update_period_ms / 1000.0;

    float speed_consign = 0.0; // [-1.0 ; 1.0]
    float speed_real = 0.0;
    const int speed_fullrange_time_ms = 1000; // 1 sec for 0->fullrange

    int deadtime_steps = 12;

    bool valid = true;

    void pin_init_pwmout(int pin) {
        gpio_set_function(pin, GPIO_FUNC_PWM);
        gpio_set_dir(pin, GPIO_OUT);
    }
    void init_pwm(int pin_low, int pin_high) {
        uint slice_num = pwm_gpio_to_slice_num(pin_low);
        if(slice_num != pwm_gpio_to_slice_num(pin_high)) {
            valid = false;
            return;
        }
        if(pwm_gpio_to_channel(pin_low) == pwm_gpio_to_channel(pin_high)) {
            valid = false;
            return;
        }
        pin_init_pwmout(pin_low);
        pin_init_pwmout(pin_high);
        pwm_set_wrap(slice_num, PWM_MAX);
        pwm_set_phase_correct(slice_num, true);
        if(pwm_gpio_to_channel(pin_low) == 0) {
            pwm_set_output_polarity(slice_num, true, false);
        } else {
            pwm_set_output_polarity(slice_num, false, true);
        }
        pwm_set_both_levels(slice_num, 0, 0);
        pwm_set_enabled(slice_num, true);
    }
    void adc_service() {
        adc_select_input(pin_temp - 26);
        float temp = adc_read() * 1.0;
        temperature_C += (temp - temperature_C) * adc_filter;

        adc_select_input(pin_current - 26);
        float cur = adc_read() * 1.0;
        current_mA += (cur - current_mA) * adc_filter;
    }
    void update_pwm() {
        if(!valid) return;
        int pwml = abs(speed_real) * (PWM_MAX + deadtime_steps + 1);
        int pwmh = MAX(0, pwml - deadtime_steps);
        if(speed_real > 0) {
            pwm_set_gpio_level(pin_AL, 0);
            pwm_set_gpio_level(pin_AH, 0);
            pwm_set_gpio_level(pin_BL, pwml);
            pwm_set_gpio_level(pin_BH, pwmh);
        } else {
            pwm_set_gpio_level(pin_BL, 0);
            pwm_set_gpio_level(pin_BH, 0);
            pwm_set_gpio_level(pin_AL, pwml);
            pwm_set_gpio_level(pin_AH, pwmh);
        }
    }
public:
    Ilotopont3(int p_al, int p_ah, int p_bl, int p_bh, int p_temp, int p_current): 
            pin_AL(p_al), pin_AH(p_ah), pin_BL(p_bl), pin_BH(p_bh), pin_temp(p_temp), pin_current(p_current) {
        init_pwm(pin_AL, pin_AH);
        init_pwm(pin_BL, pin_BH);

        adc_init();
        adc_gpio_init(pin_temp);
        adc_gpio_init(pin_current);
    }
    void service() override {
        if(!time_reached(update_timeout)) return;
        update_timeout = make_timeout_time_ms(update_period_ms);
        adc_service();
        float delta_speed = speed_consign - speed_real;
        float max_delta = update_period_ms / (float)speed_fullrange_time_ms;
        if(abs(delta_speed) >= max_delta) {
            delta_speed = copysign(max_delta, delta_speed);
        }
        speed_real += delta_speed;
        update_pwm();
    }
    void set_speed(float speed) override {
        speed_consign = std::clamp(speed, -1.0f, 1.0f);
    }
    int get_temperature_C() override {
        return (int)temperature_C;
    }
    int get_current_mA() override {
        return (int)current_mA;
    }
    void set_deadtime_ns(int ns) {
        deadtime_steps = (ns * (clock_get_hz(clk_sys) / 1000)) / 1e6;
        fraise_printf("deadtime_steps %d\n", deadtime_steps);
    }
    void receivechars(const char *data, uint8_t len) override {
        char command = data[0];
        len -= 1; data += 1;
        switch(command) {
        case 'S': // Speed
            {
                int ispeed;
                sscanf(data, "%04X", &ispeed);
                float speed = ((int16_t)ispeed) / 1000.0;
                fraise_printf("speed %f\n", speed);
                set_speed(speed);
            }
            break;
        case 'd': // dead time ns
            {
                int deadtime;
                sscanf(data, "%04X", &deadtime);
                fraise_printf("deadtime_ns %d\n", deadtime);
                set_deadtime_ns(deadtime);
            }
            break;
        case 's': // get stats
            fraise_printf("M temp: %d cur: %d\n", get_temperature_C(), get_current_mA());
            break;
        }
    }
};

