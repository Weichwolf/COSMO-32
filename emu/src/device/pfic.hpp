#pragma once
#include "../bus.hpp"
#include <array>
#include <cstdint>

namespace cosmo {

// PFIC - Programmable Fast Interrupt Controller (CH32V307 style)
// Base address: 0xE000_E000
//
// Simplified register map:
//   0x000 ISR0-3     - Interrupt Status (ro, 128 bits = 4 words)
//   0x020 IPR0-3     - Interrupt Pending (rw)
//   0x040 ITHRESHOLD - Interrupt Threshold
//   0x044 RESERVED
//   0x048 CFGR       - Configuration
//   0x04C GISR       - Global Interrupt Status
//   0x060 VTFIDR     - VTF Interrupt ID
//   0x080 VTFADDR0-3 - VTF Addresses
//   0x100 IENR0-3    - Interrupt Enable (set)
//   0x180 IRER0-3    - Interrupt Enable (clear)
//   0x200 IPSR0-3    - Interrupt Pending (set)
//   0x280 IPRR0-3    - Interrupt Pending (clear)
//   0x300 IACTR0-3   - Interrupt Active
//   0x400 IPRIOR0-63 - Priority (4 bits per interrupt, 8 per word)

class PFIC : public Device {
public:
    static constexpr size_t NUM_INTERRUPTS = 128;
    static constexpr size_t NUM_WORDS = NUM_INTERRUPTS / 32;

private:
    std::array<uint32_t, NUM_WORDS> pending_{};   // Interrupt pending
    std::array<uint32_t, NUM_WORDS> enabled_{};   // Interrupt enabled
    std::array<uint32_t, NUM_WORDS> active_{};    // Interrupt active (being serviced)
    std::array<uint8_t, NUM_INTERRUPTS> priority_{}; // 4-bit priority per IRQ

    uint32_t threshold_ = 0;  // Only IRQs with priority < threshold are taken
    uint32_t cfgr_ = 0;

public:
    uint32_t read(uint32_t addr, Width) override {
        // ISR - Status (same as pending for simplicity)
        if (addr >= 0x000 && addr < 0x010) {
            return pending_[(addr - 0x000) / 4];
        }
        // IPR - Pending
        if (addr >= 0x020 && addr < 0x030) {
            return pending_[(addr - 0x020) / 4];
        }
        // Threshold
        if (addr == 0x040) return threshold_;
        // CFGR
        if (addr == 0x048) return cfgr_;
        // GISR - Global status
        if (addr == 0x04C) {
            uint32_t any_pending = 0;
            for (size_t i = 0; i < NUM_WORDS; i++) {
                if (pending_[i] & enabled_[i]) any_pending = 1;
            }
            return any_pending;
        }
        // IENR - Enabled
        if (addr >= 0x100 && addr < 0x110) {
            return enabled_[(addr - 0x100) / 4];
        }
        // IACTR - Active
        if (addr >= 0x300 && addr < 0x310) {
            return active_[(addr - 0x300) / 4];
        }
        // IPRIOR - Priority
        if (addr >= 0x400 && addr < 0x480) {
            size_t word_idx = (addr - 0x400) / 4;
            size_t base_irq = word_idx * 8;
            uint32_t result = 0;
            for (size_t i = 0; i < 8 && base_irq + i < NUM_INTERRUPTS; i++) {
                result |= (priority_[base_irq + i] & 0xF) << (i * 4);
            }
            return result;
        }

        return 0;
    }

    void write(uint32_t addr, Width, uint32_t val) override {
        // IPR - Pending (direct write)
        if (addr >= 0x020 && addr < 0x030) {
            pending_[(addr - 0x020) / 4] = val;
            return;
        }
        // Threshold
        if (addr == 0x040) {
            threshold_ = val & 0xFF;
            return;
        }
        // CFGR
        if (addr == 0x048) {
            cfgr_ = val;
            return;
        }
        // IENR - Enable set
        if (addr >= 0x100 && addr < 0x110) {
            enabled_[(addr - 0x100) / 4] |= val;
            return;
        }
        // IRER - Enable clear
        if (addr >= 0x180 && addr < 0x190) {
            enabled_[(addr - 0x180) / 4] &= ~val;
            return;
        }
        // IPSR - Pending set
        if (addr >= 0x200 && addr < 0x210) {
            pending_[(addr - 0x200) / 4] |= val;
            return;
        }
        // IPRR - Pending clear
        if (addr >= 0x280 && addr < 0x290) {
            pending_[(addr - 0x280) / 4] &= ~val;
            return;
        }
        // IACTR - Active (typically read-only, but allow clear)
        if (addr >= 0x300 && addr < 0x310) {
            active_[(addr - 0x300) / 4] &= ~val;
            return;
        }
        // IPRIOR - Priority
        if (addr >= 0x400 && addr < 0x480) {
            size_t word_idx = (addr - 0x400) / 4;
            size_t base_irq = word_idx * 8;
            for (size_t i = 0; i < 8 && base_irq + i < NUM_INTERRUPTS; i++) {
                priority_[base_irq + i] = (val >> (i * 4)) & 0xF;
            }
            return;
        }
    }

    // Set interrupt pending (called by peripherals)
    void set_pending(uint32_t irq) {
        if (irq < NUM_INTERRUPTS) {
            pending_[irq / 32] |= (1U << (irq % 32));
        }
    }

    // Clear interrupt pending
    void clear_pending(uint32_t irq) {
        if (irq < NUM_INTERRUPTS) {
            pending_[irq / 32] &= ~(1U << (irq % 32));
        }
    }

    // Check if interrupt is enabled
    bool is_enabled(uint32_t irq) const {
        if (irq >= NUM_INTERRUPTS) return false;
        return enabled_[irq / 32] & (1U << (irq % 32));
    }

    // Check if interrupt is pending
    bool is_pending(uint32_t irq) const {
        if (irq >= NUM_INTERRUPTS) return false;
        return pending_[irq / 32] & (1U << (irq % 32));
    }

    // Get highest priority pending and enabled interrupt
    // Returns -1 if none, otherwise IRQ number
    int get_pending_irq() const {
        int best_irq = -1;
        uint8_t best_prio = 0xFF;

        for (size_t i = 0; i < NUM_WORDS; i++) {
            uint32_t pending_enabled = pending_[i] & enabled_[i];
            if (pending_enabled == 0) continue;

            for (size_t bit = 0; bit < 32; bit++) {
                if (pending_enabled & (1U << bit)) {
                    size_t irq = i * 32 + bit;
                    uint8_t prio = priority_[irq];

                    // Check threshold
                    if (prio >= threshold_ && threshold_ != 0) continue;

                    // Check if this is higher priority (lower number = higher priority)
                    if (prio < best_prio) {
                        best_prio = prio;
                        best_irq = static_cast<int>(irq);
                    }
                }
            }
        }

        return best_irq;
    }

    // Mark interrupt as being serviced
    void set_active(uint32_t irq) {
        if (irq < NUM_INTERRUPTS) {
            active_[irq / 32] |= (1U << (irq % 32));
            // Optionally clear pending when active
            clear_pending(irq);
        }
    }

    // Clear active status (called on return from interrupt)
    void clear_active(uint32_t irq) {
        if (irq < NUM_INTERRUPTS) {
            active_[irq / 32] &= ~(1U << (irq % 32));
        }
    }

    // Check if any interrupt is active
    bool any_active() const {
        for (auto a : active_) {
            if (a) return true;
        }
        return false;
    }

    // Enable an interrupt
    void enable_irq(uint32_t irq) {
        if (irq < NUM_INTERRUPTS) {
            enabled_[irq / 32] |= (1U << (irq % 32));
        }
    }

    // Disable an interrupt
    void disable_irq(uint32_t irq) {
        if (irq < NUM_INTERRUPTS) {
            enabled_[irq / 32] &= ~(1U << (irq % 32));
        }
    }
};

} // namespace cosmo
