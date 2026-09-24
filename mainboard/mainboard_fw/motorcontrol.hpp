/**
 * Motor control interface
 */

#pragma once
#include "fraise.hpp"

class MotorControl {
public:
    virtual void receivechars(const char *data, uint8_t len) {}
    virtual void service() {}
    virtual void set_speed(float speed) = 0; // speed in [-1.0;1.0]
    virtual int get_temperature_C() {return 0;}
    virtual int get_current_mA() {return 0;}
    virtual ~MotorControl() {}
};

