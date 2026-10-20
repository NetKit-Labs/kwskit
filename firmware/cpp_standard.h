#pragma once

#ifdef __cplusplus
#if __cplusplus < 201703L
#error "Project code requires C++17 or later; use make build or configure -std=gnu++17."
#endif

// Preinclude before Arduino defines abs: that macro conflicts with C++17 chrono::abs.
#include <chrono>
#endif
