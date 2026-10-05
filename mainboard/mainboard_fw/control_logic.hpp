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
#include <vector>
#include "settings_partition.hpp"

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
    static constexpr int NUM_STEPS = 6;
    static constexpr float lamp_value_tab[NUM_STEPS] = {0.1, 0.2, 0.35, 0.5, 0.75, 1.0};
    static constexpr int lamp_time_ms_tab[NUM_STEPS] = {5000, 2500, 1200, 750, 300, 0};

    uint8_t lamps_maxvalue[4] = {5, 5, 5, 5};
    uint8_t lamps_upspeed[4] = {1, 1, 1, 1};
    uint8_t lamps_downspeed[4] = {2, 2, 2, 2};
    uint8_t motor_speed[4] = {3, 1, 3, 5}; // reverse_full, reverse_slow, forward_slow, forward_full

    float get_lamp_maxvalue(int lamp) {
        return lamp_value_tab[lamps_maxvalue[std::clamp(lamp, 0, 3)]];
    }
    float get_lamp_uptime_ms(int lamp) {
        return lamp_time_ms_tab[lamps_upspeed[std::clamp(lamp, 0, 3)]];
    }
    float get_lamp_downtime_ms(int lamp) {
        return lamp_time_ms_tab[lamps_downspeed[std::clamp(lamp, 0, 3)]];
    }
    float get_motor_speed(int num_speed) {
        // 0.166 0.333 0.5 0.666 0.833 1
        return (1.0 + motor_speed[std::clamp(num_speed, 0, 3)]) / (float)NUM_STEPS;
    }
};

template<class T>
class Sequencer {
private:
    unsigned count = 0;
    std::vector<bool> sequence;
    absolute_time_t timeout = 0;
    int period_ms = 250;
public:
    void init(std::vector<T> seq, int ms) {
        period_ms = ms;
        sequence = seq;
        count = 0;
        timeout = make_timeout_time_ms(period_ms);
    }
    T get() {
        if(time_reached(timeout)) {
            timeout = make_timeout_time_ms(period_ms);
            count++;
            if(count == sequence.size()) count = 0;
        }
        return sequence[count];
    }
    Sequencer<T>(std::vector<T> seq = {0}, int ms = 250) : sequence(seq), period_ms(ms) {}
};

template <unsigned PIXELS, unsigned LAMPS> 
class TrifoilLogic : public ControlLogic<PIXELS, LAMPS> {
public:
    static constexpr int settings_slot_size = 24;
    static_assert(settings_slot_size >= sizeof(TrifoilSettings));

    using CL = ControlLogic<PIXELS, LAMPS>;
    using CL::barrette;
    using CL::motor;
    using CL::lamps;
    using CL::update_period_ms;
    using CL::enable;

private:
    enum class Button {l1 = 0, l2, reverse, stop, forward, l3, l4, count};
    static const int buttons_count = (int)Button::count;
    static const int pixels_per_button = PIXELS / buttons_count;
    enum class MotorState {reverse_full, reverse_slow, stop, forward_slow, forward_full} motor_state = MotorState::stop;
    TrifoilSettings settings;
    bool buttons[buttons_count] = {0};
    bool lamps_on[4] = {false, false, false, false};
    float lamps_value[4] = {0.0, 0.0, 0.0, 0.0}; // 0.0 -> 1.0
    const int led_off_level = 5;
    int speed_led_ms[2] = {500, 250}; // slow / fast
    Sequencer<bool> led_flasher;
    Button edited_button = Button::stop;
    int edit_step = 0;
    uint8_t *edited_value = &settings.lamps_maxvalue[0];
    SettingsPartition<settings_slot_size> &settings_partition;

    bool is_editing() {
        return edited_button != Button::stop;
    }

    int button_to_lamp(Button button) {
        switch(button) {
        case Button::l1: return 0; break;
        case Button::l2: return 1; break;
        case Button::l3: return 2; break;
        case Button::l4: return 3; break;
        default: return -1;
        }
    }

    int pixel_to_button(int pixel) {
        int button = pixel / pixels_per_button;
        if(button >= buttons_count) return -1; // forget last pixel (35)
        int button_sub = pixel % pixels_per_button;
        if(button_sub == 0 || button_sub == pixels_per_button - 1) return -1; // forget first and last pixel in a row
        return button;
    }

    int pixel_to_subbutton(int pixel, Button button) {
        int on_button = pixel / pixels_per_button;
        if(on_button != (int)button) return -1;
        return pixel % pixels_per_button;
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
        if(!is_editing()) {
            button_changed_run(button, value);
        } else {
            button_changed_edit(button, value);
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
                barrette.set_led(pixel, 0, led_flasher.get() * (255 - led_off_level) + led_off_level, 0);
            } else barrette.set_led(pixel, 0, led_off_level, 0);
            break;
        case Button::forward:
            if(motor_state == MotorState::forward_full || motor_state == MotorState::forward_slow) {
                barrette.set_led(pixel, 0, led_flasher.get() * (255 - led_off_level) + led_off_level, 0);
            } else barrette.set_led(pixel, 0, led_off_level, 0);
            break;
        default: ;
        }
    }

    void update_run() {
        for(int i = 0; i < 4; i++) {
            float dv = update_period_ms;
            dv /= lamps_on[i] ? settings.get_lamp_uptime_ms(i) : -settings.get_lamp_downtime_ms(i);
            lamps_value[i] = std::clamp(lamps_value[i] + dv, 0.0f, 1.0f);
            lamps[i].set(lamps_value[i] * settings.get_lamp_maxvalue(i));
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
        int ms = speed_led_ms[0];
        switch(motor_state) {
        case MotorState::stop:
            motor.set_speed(0.0);
            break;
        case MotorState::reverse_full:
            motor.set_speed((float)settings.get_motor_speed(0));
            ms = speed_led_ms[1];
            break;
        case MotorState::reverse_slow:
            motor.set_speed((float)settings.get_motor_speed(1));
            break;
        case MotorState::forward_slow:
            motor.set_speed((float)settings.get_motor_speed(2));
            break;
        case MotorState::forward_full:
            motor.set_speed((float)settings.get_motor_speed(3));
            ms = speed_led_ms[1];
            break;
        }
        led_flasher.init({true, false}, ms);
    }

    void button_changed_run(Button button, bool value) {
        if(!value) return;
        if(buttons[(int)Button::stop]) {
            if(button != Button::stop) {
                edit_set_button_step(button, 0);
                return;
            }
        }

        switch(button) {
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
        bool flash = led_flasher.get();
        for(unsigned  i = 0; i < PIXELS; i++) {
            int button = pixel_to_button(i);
            int sub = pixel_to_subbutton(i, edited_button);
            if(sub >= 0 && (sub < *edited_value)) {
                barrette.set_led(i, 255, 255, 255);
                continue;
            }
            switch((Button)button) {
            case Button::stop:
                barrette.set_led(i, flash ? 255 : led_off_level, 0, 0);
                break;
            case Button::reverse:
            case Button::forward:
                if(buttons[button]) barrette.set_led(i, 0, 0, 128);
                else barrette.set_led(i, 0, 0, led_off_level);
                break;
            default: barrette.set_led(i, 0);
            }
        }
    }

    void edit_set_button_step(Button button, int step) {
        static const std::vector<bool> seqs[3]{
            {true, false, false, false}, 
            {true, false, true, false, false, false},
            {true, false, true, false, true, false, false, false}
        };
        edited_button = button;
        edit_step = step;
        if(edit_step > 2) edit_step = 2;
        led_flasher.init(seqs[edit_step], 200);
        int num_lamp = button_to_lamp(button);
        if(num_lamp != -1) { // lamp
            switch(edit_step) {
            case 0: edited_value = &settings.lamps_maxvalue[num_lamp]; break;
            case 1: edited_value = &settings.lamps_upspeed[num_lamp]; break;
            case 2: edited_value = &settings.lamps_downspeed[num_lamp]; break;
            }
            return;
        } else { // motor
            if(edited_button == Button::reverse) edited_value = &settings.motor_speed[edit_step == 0 ? 1 : 0];
            else edited_value = &settings.motor_speed[edit_step == 0 ? 2 : 3];
        }
    }

    void button_changed_edit(Button button, bool value) {
        if(!value) return;
        if(button == Button::stop) {
            if(edit_step >= (button_to_lamp(edited_button) >= 0 ? 2 : 1)) {
                edited_button = Button::stop;
                return;
            }
            edit_set_button_step(edited_button, edit_step + 1);
        }
        int delta = 0;
        if(button == Button::reverse) delta = -1;
        else if(button == Button::forward) delta = 1;
        else return;
        *edited_value = std::clamp(*edited_value + delta, 0, TrifoilSettings::NUM_STEPS - 1);
    }

public:

    TrifoilLogic(Barrette<PIXELS> &barrette, MotorControl &motor, Lamp (&lamps)[LAMPS], SettingsPartition<settings_slot_size> &settings_partition) :
        ControlLogic<PIXELS, LAMPS>(barrette, motor, lamps), settings_partition(settings_partition)
    {
    }


    void do_service() override {
        update_buttons();
        if(!is_editing()) {
            update_run();
        } else {
            update_edit();
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
