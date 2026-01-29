#pragma once
#include "../bus.hpp"
#include <chrono>
#include <cstdint>

namespace cosmo {

// Host-synchronized Clock
// Provides real-time microseconds from host system
//
// Registers:
//   0x00 TIME_US_LO  (ro) - Latched microseconds, low 32 bits
//   0x04 TIME_US_HI  (ro) - Latched microseconds, high 32 bits
//   0x08 TIME_LATCH  (wo) - Write any value to latch current time
//
// Usage: Write to LATCH, then read LO and HI for atomic 64-bit value

class HostClock : public Device {
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_;
    uint64_t latched_us_ = 0;

public:
    HostClock() : start_(Clock::now()) {}

    uint32_t read(uint32_t addr, Width) override {
        switch (addr) {
            case 0x00: return static_cast<uint32_t>(latched_us_);
            case 0x04: return static_cast<uint32_t>(latched_us_ >> 32);
            default:   return 0;
        }
    }

    void write(uint32_t addr, Width, uint32_t) override {
        if (addr == 0x08) {
            auto now = Clock::now();
            latched_us_ = std::chrono::duration_cast<std::chrono::microseconds>(
                now - start_).count();
        }
    }

    // Direct access for debugging
    uint64_t current_us() const {
        auto now = Clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now - start_).count();
    }
};

} // namespace cosmo
