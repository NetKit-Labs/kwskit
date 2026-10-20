#include "device_control.h"
#include <Arduino.h>

void DeviceControl::begin() {
    deviceOn = false;
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
}

void DeviceControl::applyKeywordAction() {
    deviceOn = !deviceOn;
    digitalWrite(LED_BUILTIN, deviceOn ? HIGH : LOW);
}

bool DeviceControl::isOn() const {
    return deviceOn;
}
