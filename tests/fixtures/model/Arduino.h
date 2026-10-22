#pragma once

struct SerialStub {
    template <typename T> void print(const T &) {}

    template <typename T> void println(const T &) {}
};

inline SerialStub Serial;
