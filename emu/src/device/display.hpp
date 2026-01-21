// Display Control Peripheral
// Mode selection, VBlank status, Palette

#pragma once

#include "../bus.hpp"
#include <array>
#include <cstdint>

namespace cosmo {

// Display modes
enum class DisplayMode : uint32_t {
    Mode0_640x400x4bpp = 0,  // 16 colors from palette, 128KB
    Mode1_320x200x16bpp = 1  // Direct RGB565, 128KB
};

// Display register offsets
namespace DisplayReg {
    constexpr uint32_t MODE    = 0x00;  // Display mode (rw)
    constexpr uint32_t STATUS  = 0x04;  // Status register (ro)
    constexpr uint32_t PALETTE = 0x40;  // Palette base (16 x 16-bit RGB565)
}

// Status bits
namespace DisplayStatus {
    constexpr uint32_t VBLANK = 1 << 0;  // VBlank active
}

class DisplayControl : public Device {
public:
    // Display dimensions per mode
    static constexpr int MODE0_WIDTH = 640;
    static constexpr int MODE0_HEIGHT = 400;
    static constexpr int MODE1_WIDTH = 320;
    static constexpr int MODE1_HEIGHT = 200;

    // Timing (cycles)
    static constexpr uint64_t CYCLES_PER_FRAME = 144'000'000 / 60;  // 60 Hz
    static constexpr uint64_t VBLANK_CYCLES = CYCLES_PER_FRAME / 10; // 10% VBlank

    DisplayControl() {
        // Initialize default palette (grayscale ramp)
        for (int i = 0; i < 16; i++) {
            uint16_t gray = (i * 2) & 0x1F;  // 5-bit gray
            palette_[i] = (gray << 11) | (gray << 6) | gray;  // RGB565
        }
    }

    uint32_t read(uint32_t addr, Width w) override {
        addr &= 0xFF;

        if (addr == DisplayReg::MODE) {
            return static_cast<uint32_t>(mode_);
        }

        if (addr == DisplayReg::STATUS) {
            return status_;
        }

        // Palette access (16 entries, 16-bit each)
        if (addr >= DisplayReg::PALETTE && addr < DisplayReg::PALETTE + 32) {
            uint32_t idx = (addr - DisplayReg::PALETTE) / 2;
            if (idx < 16) {
                return palette_[idx];
            }
        }

        return 0;
    }

    void write(uint32_t addr, Width w, uint32_t val) override {
        addr &= 0xFF;

        if (addr == DisplayReg::MODE) {
            mode_ = static_cast<DisplayMode>(val & 1);
            return;
        }

        // STATUS is read-only

        // Palette access
        if (addr >= DisplayReg::PALETTE && addr < DisplayReg::PALETTE + 32) {
            uint32_t idx = (addr - DisplayReg::PALETTE) / 2;
            if (idx < 16) {
                palette_[idx] = val & 0xFFFF;
            }
        }
    }

    // Update VBlank status based on cycle count
    std::optional<Interrupt> tick(uint64_t cycles) override {
        uint64_t frame_cycle = cycles % CYCLES_PER_FRAME;
        uint64_t active_end = CYCLES_PER_FRAME - VBLANK_CYCLES;

        bool was_vblank = (status_ & DisplayStatus::VBLANK) != 0;
        bool is_vblank = frame_cycle >= active_end;

        if (is_vblank) {
            status_ |= DisplayStatus::VBLANK;
        } else {
            status_ &= ~DisplayStatus::VBLANK;
        }

        // Generate interrupt on VBlank start
        if (!was_vblank && is_vblank && vblank_irq_enabled_) {
            return Interrupt{VBLANK_IRQ};
        }

        return std::nullopt;
    }

    // Accessors for renderer
    DisplayMode mode() const { return mode_; }
    bool is_vblank() const { return (status_ & DisplayStatus::VBLANK) != 0; }
    const std::array<uint16_t, 16>& palette() const { return palette_; }

    // Get current dimensions
    int width() const {
        return (mode_ == DisplayMode::Mode0_640x400x4bpp) ? MODE0_WIDTH : MODE1_WIDTH;
    }

    int height() const {
        return (mode_ == DisplayMode::Mode0_640x400x4bpp) ? MODE0_HEIGHT : MODE1_HEIGHT;
    }

    void enable_vblank_irq(bool enable) { vblank_irq_enabled_ = enable; }

    static constexpr uint32_t VBLANK_IRQ = 24;  // Display VBlank IRQ number

private:
    DisplayMode mode_ = DisplayMode::Mode0_640x400x4bpp;
    uint32_t status_ = 0;
    std::array<uint16_t, 16> palette_{};
    bool vblank_irq_enabled_ = false;
};

} // namespace cosmo
