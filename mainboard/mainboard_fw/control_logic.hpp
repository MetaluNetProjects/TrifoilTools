/**
 * Trifoil control logic
 */

#pragma once
#include "motorcontrol.hpp"
#include "barrette.hpp"
#include "fraise.hpp"
#include <math.h>
#include <algorithm>

template <unsigned PIXELS> class ControlLogic {
protected:
    Barrette<PIXELS> &barrette;
    MotorControl &motor;
    const int update_period_ms = 25;
    absolute_time_t update_timeout;

public:
    ControlLogic(Barrette<PIXELS> &barrette, MotorControl &motor) : barrette(barrette), motor(motor) {
    }
    virtual ~ControlLogic() {}
    virtual void do_service() = 0;
    virtual void receivechars(const char *data, uint8_t len) {}
    void service() {
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

template <unsigned PIXELS> class TrifoilLogic : public ControlLogic<PIXELS> {
    using ControlLogic<PIXELS>::barrette;
    using ControlLogic<PIXELS>::motor;
    using ControlLogic<PIXELS>::update_period_ms;
private:
    enum class Mode {run, edit} mode = Mode::run;
    enum class Button {l1 = 0, l2, reverse, stop, forward, l3, l4, count};
    static const int buttons_count = (int)Button::count;
    static const int pixels_per_button = PIXELS / buttons_count;
    enum class MotorState {reverse_full, reverse_slow, stop, forward_slow, forward_full} motor_state = MotorState::stop;
    TrifoilSettings settings;
    bool buttons[buttons_count] = {0};
    bool prev_buttons[buttons_count] = {0};
    bool lamps_on[4] = {false, false, false, false};
    float lamps_value[4] = {0.0, 0.0, 0.0, 0.0}; // 0.0 -> 1.0

    int pixel_to_button(int pixel) {
        int button = pixel / pixels_per_button;
        if(button >= buttons_count) return -1; // forget last pixel (35)
        int button_sub = pixel % pixels_per_button;
        if(button_sub == 0 || button_sub == pixels_per_button - 1) return -1; // forget first and last pixel in a row
        return button;
    }

    void update_buttons() {
        uint64_t halls = barrette.get_halls();
        for(unsigned  i = 0; i < buttons_count; i++) {
            buttons[i] = false;
        }
        for(unsigned  i = 0; i < PIXELS; i++) {
            bool hall = (halls & (1 << i)) != 0;
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
        uint8_t v = (int)(lamps_value[lamp] * 255.0);
        barrette.set_led(pixel, v, v, 0); // yellow
    }

    void update_run() {
        for(int i = 0; i < 4; i++) {
            float dv = update_period_ms;
            dv /= lamps_on[i] ? settings.lamps_uptime_ms[i] : -settings.lamps_downtime_ms[i];
            lamps_value[i] = std::clamp(lamps_value[i] + dv, 0.0f, 1.0f);
        }
        for(unsigned  i = 0; i < PIXELS; i++) {
            int button = pixel_to_button(i);
            if(button == -1) barrette.set_led(i, 0);
            switch((Button)button) {
            case Button::l1: set_lamp_pixel_led(i, 0); break;
            case Button::l2: set_lamp_pixel_led(i, 1); break;
            case Button::l3: set_lamp_pixel_led(i, 2); break;
            case Button::l4: set_lamp_pixel_led(i, 3); break;
            default: ;
            }
        }
    }

    void button_changed_run(Button button, bool value) {
        if(value) switch(button) {
        case Button::l1: lamps_on[0] ^= true; break;
        case Button::l2: lamps_on[1] ^= true; break;
        case Button::l3: lamps_on[2] ^= true; break;
        case Button::l4: lamps_on[3] ^= true; break;
        default: ;
        }
    }

    void update_edit() {
    }

    void button_changed_edit(Button button, bool value) {
    }

public:
    TrifoilLogic(Barrette<PIXELS> &barrette, MotorControl &motor) : ControlLogic<PIXELS>(barrette, motor) {
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
};
