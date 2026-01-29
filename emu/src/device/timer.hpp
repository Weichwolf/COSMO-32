#pragma once
#include "../bus.hpp"
#include <cstdint>

namespace cosmo {

// SysTick Timer (RISC-V style, based on mtime/mtimecmp)
// Memory-mapped at 0xE000_0000 region alongside PFIC
//
// Registers:
//   0x00 CTRL    - Control (enable, interrupt enable)
//   0x04 SR      - Status (count flag)
//   0x08 CNT_LO  - Current count low
//   0x0C CNT_HI  - Current count high
//   0x10 CMP_LO  - Compare value low
//   0x14 CMP_HI  - Compare value high
//   0x18 RELOAD  - Auto-reload value (for periodic mode)
//
// Simplified compared to full mtime, optimized for SysTick use case

class SysTickTimer : public Device {
public:
    // Control register bits
    static constexpr uint32_t CTRL_ENABLE = 1 << 0;   // Timer enable
    static constexpr uint32_t CTRL_TICKINT = 1 << 1;  // Interrupt enable
    static constexpr uint32_t CTRL_MODE = 1 << 2;     // 0=one-shot, 1=periodic

    // Status register bits
    static constexpr uint32_t SR_CNTIF = 1 << 0;      // Count reached compare

    // Interrupt number for PFIC
    static constexpr uint32_t IRQ_NUM = 12;  // SysTick interrupt

private:
    uint32_t ctrl_ = 0;
    uint32_t sr_ = 0;
    uint64_t cnt_ = 0;
    uint64_t cmp_ = 0;
    uint32_t reload_ = 0;

    bool irq_pending_ = false;

public:
    uint32_t read(uint32_t addr, Width) override {
        switch (addr) {
            case 0x00: return ctrl_;
            case 0x04: return sr_;
            case 0x08: return static_cast<uint32_t>(cnt_);
            case 0x0C: return static_cast<uint32_t>(cnt_ >> 32);
            case 0x10: return static_cast<uint32_t>(cmp_);
            case 0x14: return static_cast<uint32_t>(cmp_ >> 32);
            case 0x18: return reload_;
            default:   return 0;
        }
    }

    void write(uint32_t addr, Width, uint32_t val) override {
        switch (addr) {
            case 0x00:
                ctrl_ = val;
                if (!(val & CTRL_ENABLE)) {
                    irq_pending_ = false;
                }
                break;
            case 0x04:
                // Writing clears status flags
                sr_ &= ~val;
                if (val & SR_CNTIF) {
                    irq_pending_ = false;
                }
                break;
            case 0x08:
                cnt_ = (cnt_ & 0xFFFFFFFF00000000ULL) | val;
                break;
            case 0x0C:
                cnt_ = (cnt_ & 0xFFFFFFFFULL) | (static_cast<uint64_t>(val) << 32);
                break;
            case 0x10:
                cmp_ = (cmp_ & 0xFFFFFFFF00000000ULL) | val;
                break;
            case 0x14:
                cmp_ = (cmp_ & 0xFFFFFFFFULL) | (static_cast<uint64_t>(val) << 32);
                break;
            case 0x18:
                reload_ = val;
                break;
        }
    }

    std::optional<Interrupt> tick(uint64_t) override {
        if (!(ctrl_ & CTRL_ENABLE)) {
            return {};
        }

        cnt_++;

        if (cnt_ >= cmp_ && cmp_ != 0) {
            sr_ |= SR_CNTIF;

            if (ctrl_ & CTRL_MODE) {
                // Periodic mode - reload
                cnt_ = 0;
                if (reload_ != 0) {
                    cmp_ = reload_;
                }
            } else {
                // One-shot - disable
                ctrl_ &= ~CTRL_ENABLE;
            }

            if ((ctrl_ & CTRL_TICKINT) && !irq_pending_) {
                irq_pending_ = true;
                return Interrupt{IRQ_NUM};
            }
        }

        return {};
    }

    bool has_pending_irq() const { return irq_pending_; }
    void clear_irq() { irq_pending_ = false; }

    // Direct access for testing
    uint64_t count() const { return cnt_; }
    void set_count(uint64_t v) { cnt_ = v; }
};

} // namespace cosmo
