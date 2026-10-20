#pragma once
#include "cpp_standard.h"

class DeviceControl {
public:
    DeviceControl() = default;
    DeviceControl(const DeviceControl &) = delete;
    DeviceControl &operator=(const DeviceControl &) = delete;

    void begin();
    void applyKeywordAction();
    bool isOn() const;

private:
    bool deviceOn = false;
};
