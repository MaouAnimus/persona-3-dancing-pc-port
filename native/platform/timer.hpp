#pragma once
#include <windows.h>
#include <cstdint>

namespace p3d::platform {

class HighPrecisionTimer {
public:
    HighPrecisionTimer() {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        frequency_ = freq.QuadPart;
        reset();
    }

    void reset() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        start_time_ = now.QuadPart;
        last_time_ = start_time_;
    }

    // Returns elapsed time in seconds since reset
    double elapsed_seconds() const {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return static_cast<double>(now.QuadPart - start_time_) / frequency_;
    }

    // Returns elapsed time in milliseconds since reset
    double elapsed_milliseconds() const {
        return elapsed_seconds() * 1000.0;
    }

    // Returns delta time in seconds since last call to tick()
    double tick() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        int64_t delta = now.QuadPart - last_time_;
        last_time_ = now.QuadPart;
        return static_cast<double>(delta) / frequency_;
    }

private:
    int64_t frequency_{1};
    int64_t start_time_{0};
    int64_t last_time_{0};
};

} // namespace p3d::platform
