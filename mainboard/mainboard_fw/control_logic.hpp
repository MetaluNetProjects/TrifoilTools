/**
 * Trifoil control logic
 */

#pragma once
#include "motorcontrol.hpp"
#include "barrette.hpp"
#include "lamp.hpp"
#include "fraise.hpp"
#include <math.h>
#include <algorithm>

template <unsigned PIXELS, unsigned LAMPS> class ControlLogic {
protected:
    Barrette<PIXELS> &barrette;
    MotorControl &motor;
    Lamp (&lamps)[LAMPS];
    const int update_period_ms = 25;
    absolute_time_t update_timeout;
    bool enable = true;

public:
    ControlLogic(Barrette<PIXELS> &barrette, MotorControl &motor, Lamp (&lamps)[LAMPS]) : barrette(barrette), motor(motor), lamps(lamps) {
    }
    virtual ~ControlLogic() {}
    virtual void do_service() = 0;
    virtual void receivechars(const char *data, uint8_t len) {}
    void service() {
        if(!enable) return;
        if(!time_reached(update_timeout)) return;
        update_timeout = make_timeout_time_ms(update_period_ms);
        do_service();
    }
};

struct TrifoilSettings {
    float lamps_maxvalue[4] = {1.0, 1.0, 1.0, 1.0};
    int lamps_uptime_ms[4] = {2000, 2000, 2000, 2000};
    int lamps_downtime_ms[4] = {1000, 1000, 1000, 1000};
    float motor_speed[4] = {0.5, 0.25, 0.5, 1.0}; // reverse_full, reverse_slow, forward_slow, forward_full
};

template <unsigned PIXELS, unsigned LAMPS> 
class TrifoilLogic : public ControlLogic<PIXELS, LAMPS> {
    using CL = ControlLogic<PIXELS, LAMPS>;
    using CL::barrette;
    using CL::motor;
    using CL::lamps;
    using CL::update_period_ms;
    using CL::enable;
private:
    enum class Mode {run, edit} mode = Mode::run;
    enum class Button {l1 = 0, l2, reverse, stop, forward, l3, l4, count};
    static const int buttons_count = (int)Button::count;
    static const int pixels_per_button = PIXELS / buttons_count;
    enum class MotorState {reverse_full, reverse_slow, stop, forward_slow, forward_full} motor_state = MotorState::stop;
    TrifoilSettings settings;
    bool buttons[buttons_count] = {0};
    bool lamps_on[4] = {false, false, false, false};
    float lamps_value[4] = {0.0, 0.0, 0.0, 0.0}; // 0.0 -> 1.0
    const int led_off_level = 5;
    bool speed_led_flash = false;
    int speed_led_ms[2] = {500, 250}; // slow / fast
    absolute_time_t speed_led_timeout = 0;

    bool speed_led_update(bool init = false) {
        int ms = (motor_state == MotorState::reverse_full || motor_state == MotorState::forward_full) ?
            speed_led_ms[1] : speed_led_ms[0];
        if(init) {
            speed_led_flash = true;
            speed_led_timeout = make_timeout_time_ms(ms);
        } else {
            if(time_reached(speed_led_timeout)) {
                speed_led_timeout = make_timeout_time_ms(ms);
                speed_led_flash = !speed_led_flash;
            }
        }
        return speed_led_flash;
    }

    int pixel_to_button(int pixel) {
        int button = pixel / pixels_per_button;
        if(button >= buttons_count) return -1; // forget last pixel (35)
        int button_sub = pixel % pixels_per_button;
        if(button_sub == 0 || button_sub == pixels_per_button - 1) return -1; // forget first and last pixel in a row
        return button;
    }

    void update_buttons() {
        uint64_t halls = barrette.get_halls();
        bool prev_buttons[buttons_count] = {0};
        for(unsigned  i = 0; i < buttons_count; i++) {
            prev_buttons[i] = buttons[i];
            buttons[i] = false;
        }
        for(unsigned  i = 0; i < PIXELS; i++) {
            bool hall = (halls & (uint64_t)(1LL << i)) != 0;
            if(!hall) continue;
            int button = pixel_to_button(i);
            if(button == -1) continue; // is no button
            buttons[button] = true;
        }
        for(int i = 0; i < buttons_count; i++) {
            if(prev_buttons[i] != buttons[i]) {
                button_changed((Button)i, buttons[i]);
            }
            prev_buttons[i] = buttons[i];
        }
    }

    void button_changed(Button button, bool value) {
        switch(mode) {
        case Mode::run:
            button_changed_run(button, value);
            break;
        case Mode::edit:
            button_changed_edit(button, value);
            break;
        }
    }

    void set_lamp_pixel_led(int pixel, int lamp) {
        uint8_t r = (int)(lamps_value[lamp] * (255 - led_off_level) + led_off_level);
        uint8_t g = (int)(lamps_value[lamp] * (255 - led_off_level) + led_off_level);
        barrette.set_led(pixel, r, g, 0); // yellow
    }

    void set_motor_pixel_led(Button button, int pixel) {
        switch(button) {
        case Button::reverse:
            if(motor_state == MotorState::reverse_full || motor_state == MotorState::reverse_slow) {
                barrette.set_led(pixel, 0, speed_led_update() * (255 - led_off_level) + led_off_level, 0);
            } else barrette.set_led(pixel, 0, led_off_level, 0);
            break;
        case Button::forward:
            if(motor_state == MotorState::forward_full || motor_state == MotorState::forward_slow) {
                barrette.set_led(pixel, 0, speed_led_update() * (255 - led_off_level) + led_off_level, 0);
            } else barrette.set_led(pixel, 0, led_off_level, 0);
            break;
        default: ;
        }
    }

    void update_run() {
        for(int i = 0; i < 4; i++) {
            float dv = update_period_ms;
            dv /= lamps_on[i] ? settings.lamps_uptime_ms[i] : -settings.lamps_downtime_ms[i];
            lamps_value[i] = std::clamp(lamps_value[i] + dv, 0.0f, 1.0f);
            lamps[i].set(lamps_value[i] * settings.lamps_maxvalue[i]);
        }
        for(unsigned  i = 0; i < PIXELS; i++) {
            int button = pixel_to_button(i);
            if(button == -1) {
                barrette.set_led(i, 0);
                continue;
            }
            switch((Button)button) {
            case Button::l1: set_lamp_pixel_led(i, 0); break;
            case Button::l2: set_lamp_pixel_led(i, 1); break;
            case Button::l3: set_lamp_pixel_led(i, 2); break;
            case Button::l4: set_lamp_pixel_led(i, 3); break;
            case Button::reverse: set_motor_pixel_led((Button)button, i); break;
            case Button::forward: set_motor_pixel_led((Button)button, i); break;
            case Button::stop: barrette.set_led(i, motor_state == MotorState::stop ? 255 : led_off_level, 0, 0); break;
            default: ;
            }
        }
    }

    void set_motor_state(MotorState state) {
        motor_state = state;
        switch(motor_state) {
        case MotorState::stop:
            motor.set_speed(0.0);
            break;
        case MotorState::reverse_full:
            motor.set_speed(settings.motor_speed[0]);
            break;
        case MotorState::reverse_slow:
            motor.set_speed(settings.motor_speed[1]);
            break;
        case MotorState::forward_slow:
            motor.set_speed(settings.motor_speed[2]);
            break;
        case MotorState::forward_full:
            motor.set_speed(settings.motor_speed[3]);
            break;
        }
        speed_led_update(true);
    }

    void button_changed_run(Button button, bool value) {
        if(value) switch(button) {
        case Button::l1: lamps_on[0] ^= true; break;
        case Button::l2: lamps_on[1] ^= true; break;
        case Button::l3: lamps_on[2] ^= true; break;
        case Button::l4: lamps_on[3] ^= true; break;
        case Button::stop: 
            set_motor_state(MotorState::stop);
            break;
        case Button::forward: 
            if(motor_state == MotorState::forward_slow) set_motor_state(MotorState::forward_full);
            else set_motor_state(MotorState::forward_slow);
            break;
        case Button::reverse: 
            if(motor_state == MotorState::reverse_slow) set_motor_state(MotorState::reverse_full);
            else set_motor_state(MotorState::reverse_slow);
            break;
        default: ;
        }
    }

    void update_edit() {
    }

    void button_changed_edit(Button button, bool value) {
    }

public:
    TrifoilLogic(Barrette<PIXELS> &barrette, MotorControl &motor, Lamp (&lamps)[LAMPS]) : ControlLogic<PIXELS, LAMPS>(barrette, motor, lamps) {
    }

    void do_service() override {
        update_buttons();
        switch(mode) {
        case Mode::run:
            update_run();
            break;
        case Mode::edit:
            update_edit();
            break;
        }
    }
    void receivechars(const char *data, uint8_t len) override {
        char command = data[0];
        len -= 1; data += 1;
        switch(command) {
        case 'D': { // debug 
                uint64_t halls = barrette.get_halls();
                fraise_printf("H%016llX\n", halls);
                fraise_printf("bits ");
                for(unsigned  i = 0; i < PIXELS; i++) {
                    //int button = pixel_to_button(i);
                    if((halls & (1LL << i)) != 0) fraise_printf("1 ");
                    else fraise_printf("0 ");
                }
                fraise_printf("\n");
            }
            break;
        case 'e': // enable
            enable = data[0] != '0';
        }
    }
};
