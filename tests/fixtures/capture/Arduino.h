#pragma once

#include <algorithm>
#include <cstdint>
using std::min;
std::uint32_t micros();
void noInterrupts();
void interrupts();
std::uint32_t __get_PRIMASK();
void __set_PRIMASK(std::uint32_t mask);
