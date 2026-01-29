#pragma once
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>
#include <optional>

namespace cosmo {

enum class Width { Byte, Half, Word };

struct Interrupt {
    uint32_t cause;
};

struct Device {
    virtual ~Device() = default;
    virtual uint32_t read(uint32_t addr, Width w) = 0;
    virtual void write(uint32_t addr, Width w, uint32_t val) = 0;
    virtual std::optional<Interrupt> tick(uint64_t cycles) { return {}; }
};

struct DeviceMapping {
    uint32_t base;
    uint32_t size;
    Device* device;
};

class Bus {
    std::vector<DeviceMapping> mappings_;

    // Fast-path with direct data pointers (no virtual calls)
    uint8_t* flash_data_ = nullptr;
    uint32_t flash_end_ = 0;
    uint8_t* sram_data_ = nullptr;
    uint32_t sram_base_ = 0, sram_end_ = 0;

public:
    void map(uint32_t base, uint32_t size, Device* dev) {
        mappings_.push_back({base, size, dev});
    }

    void set_fast_path(uint8_t* flash, uint32_t fs,
                       uint8_t* sram, uint32_t sb, uint32_t ss) {
        flash_data_ = flash; flash_end_ = fs;
        sram_data_ = sram; sram_base_ = sb; sram_end_ = sb + ss;
    }

    Device* find(uint32_t addr) const {
        for (auto& m : mappings_) {
            if (addr >= m.base && addr < m.base + m.size)
                return m.device;
        }
        return nullptr;
    }

    uint32_t offset(uint32_t addr) const {
        for (auto& m : mappings_) {
            if (addr >= m.base && addr < m.base + m.size)
                return addr - m.base;
        }
        return addr;
    }

    uint32_t read(uint32_t addr, Width w) {
        // Fast-path for flash (inline, no virtual call)
        if (addr < flash_end_) {
            switch (w) {
                case Width::Byte: return flash_data_[addr];
                case Width::Half: return *reinterpret_cast<uint16_t*>(flash_data_ + addr);
                case Width::Word: return *reinterpret_cast<uint32_t*>(flash_data_ + addr);
            }
        }
        // Fast-path for sram (inline, no virtual call)
        if (addr >= sram_base_ && addr < sram_end_) {
            uint8_t* p = sram_data_ + (addr - sram_base_);
            switch (w) {
                case Width::Byte: return *p;
                case Width::Half: return *reinterpret_cast<uint16_t*>(p);
                case Width::Word: return *reinterpret_cast<uint32_t*>(p);
            }
        }
        // Slow path for peripherals
        for (auto& m : mappings_) {
            if (addr >= m.base && addr < m.base + m.size)
                return m.device->read(addr - m.base, w);
        }
        std::fprintf(stderr, "[BUS] Unmapped read: 0x%08X\n", addr);
        return 0;
    }

    void write(uint32_t addr, Width w, uint32_t val) {
        // Fast-path for sram (inline, no virtual call)
        if (addr >= sram_base_ && addr < sram_end_) {
            uint8_t* p = sram_data_ + (addr - sram_base_);
            switch (w) {
                case Width::Byte: *p = val; return;
                case Width::Half: *reinterpret_cast<uint16_t*>(p) = val; return;
                case Width::Word: *reinterpret_cast<uint32_t*>(p) = val; return;
            }
        }
        // Slow path for peripherals
        for (auto& m : mappings_) {
            if (addr >= m.base && addr < m.base + m.size) {
                m.device->write(addr - m.base, w, val);
                return;
            }
        }
        std::fprintf(stderr, "[BUS] Unmapped write: 0x%08X = 0x%08X\n", addr, val);
    }

    uint32_t read8(uint32_t addr) { return read(addr, Width::Byte); }
    uint32_t read16(uint32_t addr) { return read(addr, Width::Half); }
    uint32_t read32(uint32_t addr) { return read(addr, Width::Word); }
    void write8(uint32_t addr, uint32_t v) { write(addr, Width::Byte, v); }
    void write16(uint32_t addr, uint32_t v) { write(addr, Width::Half, v); }
    void write32(uint32_t addr, uint32_t v) { write(addr, Width::Word, v); }
};

} // namespace cosmo
