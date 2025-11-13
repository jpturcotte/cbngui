#pragma once

#include <initializer_list>

enum class DebugLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error
};

template <typename... Args>
inline void debuglog(DebugLevel level, Args&&... args) {
    (void)level;
    (void)std::initializer_list<int>{((void)args, 0)...};
}

