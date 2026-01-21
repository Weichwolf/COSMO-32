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

public:
    void map(uint32_t base, uint32_t size, Device* dev) {
        mappings_.push_back({base, size, dev});
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
        if (auto* dev = find(addr))
            return dev->read(offset(addr), w);
        std::fprintf(stderr, "[BUS] Unmapped read: 0x%08X\n", addr);
        return 0;
    }

    void write(uint32_t addr, Width w, uint32_t val) {
        if (auto* dev = find(addr))
            dev->write(offset(addr), w, val);
        else
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
