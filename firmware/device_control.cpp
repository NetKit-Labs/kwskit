#include "device_control.h"
#include <Arduino.h>
namespace DeviceControl {
    namespace {
        bool deviceOn = false;
    }
    void begin() {
        deviceOn = false;
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, LOW);
    }
    void applyKeywordAction() {
        deviceOn = !deviceOn;
        digitalWrite(LED_BUILTIN, deviceOn ? HIGH : LOW);
    }
    bool isOn() {
        return deviceOn;
    }
} // namespace DeviceControl
