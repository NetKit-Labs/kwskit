#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#define PI 3.1415926535897932384626433832795
#define constrain(value, low, high) \
    ((value) < (low) ? (low) : ((value) > (high) ? (high) : (value)))
