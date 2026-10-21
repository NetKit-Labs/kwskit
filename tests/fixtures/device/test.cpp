#include <cassert>

#include "device_control.h"
int pin = -1, level = -1, mode = -1;

void pinMode(int p, int m) {
    pin = p;
    mode = m;
}

void digitalWrite(int p, int v) {
    pin = p;
    level = v;
}

int main() {
    DeviceControl device;
    device.begin();
    assert(pin == 13 && mode == 1 && level == 0 && !device.isOn());
    device.applyKeywordAction();
    assert(level == 1 && device.isOn());
    device.applyKeywordAction();
    assert(level == 0 && !device.isOn());
}
