// FSMC - Flexible Static Memory Controller
// External 1MB SRAM (IS62WV102416)

#pragma once

#include "../bus.hpp"
#include <cstdint>
#include <cstring>
#include <vector>

namespace cosmo {

class FSMC : public Device {
public:
    static constexpr uint32_t SIZE = 0x100000;  // 1MB
    static constexpr uint32_t FRAMEBUFFER_OFFSET = 0xE0000;  // 896KB offset
    static constexpr uint32_t FRAMEBUFFER_SIZE = 0x20000;    // 128KB

    FSMC() : memory_(SIZE, 0) {}

    uint32_t read(uint32_t addr, Width w) override {
        addr &= (SIZE - 1);

        switch (w) {
            case Width::Byte:
                return memory_[addr];
            case Width::Half:
                if (addr + 1 < SIZE) {
                    return memory_[addr] | (memory_[addr + 1] << 8);
                }
                return memory_[addr];
            case Width::Word:
                if (addr + 3 < SIZE) {
                    return memory_[addr] |
                           (memory_[addr + 1] << 8) |
                           (memory_[addr + 2] << 16) |
                           (memory_[addr + 3] << 24);
                }
                return 0;
        }
        return 0;
    }

    void write(uint32_t addr, Width w, uint32_t val) override {
        addr &= (SIZE - 1);

        switch (w) {
            case Width::Byte:
                memory_[addr] = val & 0xFF;
                break;
            case Width::Half:
                if (addr + 1 < SIZE) {
                    memory_[addr] = val & 0xFF;
                    memory_[addr + 1] = (val >> 8) & 0xFF;
                }
                break;
            case Width::Word:
                if (addr + 3 < SIZE) {
                    memory_[addr] = val & 0xFF;
                    memory_[addr + 1] = (val >> 8) & 0xFF;
                    memory_[addr + 2] = (val >> 16) & 0xFF;
                    memory_[addr + 3] = (val >> 24) & 0xFF;
                }
                break;
        }
    }

    // Direct access to framebuffer for rendering
    const uint8_t* framebuffer() const {
        return memory_.data() + FRAMEBUFFER_OFFSET;
    }

    uint8_t* framebuffer() {
        return memory_.data() + FRAMEBUFFER_OFFSET;
    }

    // Direct memory access for DMA
    const uint8_t* data() const { return memory_.data(); }
    uint8_t* data() { return memory_.data(); }

private:
    std::vector<uint8_t> memory_;
};

} // namespace cosmo
