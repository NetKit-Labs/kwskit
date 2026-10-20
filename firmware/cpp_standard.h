#pragma once

#ifdef __cplusplus
#if __cplusplus < 202002L
#error "Project code requires C++20 or later; use make build or configure -std=gnu++20."
#endif

// Preinclude before Arduino defines abs: that macro conflicts with C++20 chrono::abs.
#include <chrono>
#endif
