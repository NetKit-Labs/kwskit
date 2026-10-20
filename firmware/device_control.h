#pragma once

class DeviceControl {
public:
    void begin();
    void applyKeywordAction();
    bool isOn() const;

private:
    bool deviceOn = false;
};
