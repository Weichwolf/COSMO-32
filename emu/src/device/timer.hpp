#pragma once
#include "../bus.hpp"
#include <chrono>
#include <cstdint>

namespace cosmo {

// RISC-V mtime/mtimecmp style timer
// Memory-mapped at 0xE000_0000
//
// Registers:
//   0x00 MTIME_LO  - Current time low (ms since boot)
//   0x04 MTIME_HI  - Current time high
//   0x08 MTIMECMP_LO - Compare value low
//   0x0C MTIMECMP_HI - Compare value high
//
// Timer uses wall-clock time for accurate uptime display

class SysTickTimer : public Device {
public:
    static constexpr uint32_t IRQ_NUM = 7;  // Machine timer interrupt

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point start_time_ = Clock::now();
    uint64_t mtimecmp_ = 0;
    bool irq_pending_ = false;

    uint64_t mtime_ms() const {
        auto now = Clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
    }

public:
    uint32_t read(uint32_t addr, Width) override {
        uint64_t mtime = mtime_ms();
        switch (addr) {
            case 0x00: return static_cast<uint32_t>(mtime);
            case 0x04: return static_cast<uint32_t>(mtime >> 32);
            case 0x08: return static_cast<uint32_t>(mtimecmp_);
            case 0x0C: return static_cast<uint32_t>(mtimecmp_ >> 32);
            default:   return 0;
        }
    }

    void write(uint32_t addr, Width, uint32_t val) override {
        switch (addr) {
            case 0x00:
                // Writing mtime resets the start time (offset)
                start_time_ = Clock::now();
                break;
            case 0x04:
                // Ignore high word write for simplicity
                break;
            case 0x08:
                mtimecmp_ = (mtimecmp_ & 0xFFFFFFFF00000000ULL) | val;
                irq_pending_ = false;
                break;
            case 0x0C:
                mtimecmp_ = (mtimecmp_ & 0xFFFFFFFFULL) | (static_cast<uint64_t>(val) << 32);
                break;
        }
    }

    std::optional<Interrupt> tick(uint64_t) override {
        if (mtimecmp_ != 0 && mtime_ms() >= mtimecmp_ && !irq_pending_) {
            irq_pending_ = true;
            return Interrupt{IRQ_NUM};
        }
        return {};
    }

    bool has_pending_irq() const { return irq_pending_; }
    void clear_irq() { irq_pending_ = false; }

    uint64_t count() const { return mtime_ms(); }
    void set_count(uint64_t) { start_time_ = Clock::now(); }
};

} // namespace cosmo
