#pragma once
#include "../bus.hpp"
#include <vector>
#include <cstring>
#include <fstream>

namespace cosmo {

class RAM : public Device {
    std::vector<uint8_t> data_;

public:
    explicit RAM(size_t size) : data_(size, 0) {}

    uint8_t* data() { return data_.data(); }
    size_t size() const { return data_.size(); }

    uint32_t read(uint32_t addr, Width w) override {
        if (addr >= data_.size()) return 0;
        uint32_t val = 0;
        switch (w) {
            case Width::Byte: val = data_[addr]; break;
            case Width::Half:
                if (addr + 1 >= data_.size()) return 0;
                val = data_[addr] | (data_[addr + 1] << 8);
                break;
            case Width::Word:
                if (addr + 3 >= data_.size()) return 0;
                val = data_[addr] | (data_[addr + 1] << 8) |
                       (data_[addr + 2] << 16) | (data_[addr + 3] << 24);
                break;
        }
        return val;
    }

    void write(uint32_t addr, Width w, uint32_t val) override {
        if (addr >= data_.size()) return;
        switch (w) {
            case Width::Byte:
                data_[addr] = val & 0xFF;
                break;
            case Width::Half:
                if (addr + 1 >= data_.size()) return;
                data_[addr] = val & 0xFF;
                data_[addr + 1] = (val >> 8) & 0xFF;
                break;
            case Width::Word:
                if (addr + 3 >= data_.size()) return;
                data_[addr] = val & 0xFF;
                data_[addr + 1] = (val >> 8) & 0xFF;
                data_[addr + 2] = (val >> 16) & 0xFF;
                data_[addr + 3] = (val >> 24) & 0xFF;
                break;
        }
    }

    void load(const void* src, size_t len, size_t offset = 0) {
        if (offset + len > data_.size()) len = data_.size() - offset;
        std::memcpy(data_.data() + offset, src, len);
    }
};

class ROM : public Device {
    std::vector<uint8_t> data_;

public:
    explicit ROM(size_t size) : data_(size, 0) {}

    uint8_t* data() { return data_.data(); }
    size_t size() const { return data_.size(); }

    bool load_file(const char* path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        f.seekg(0, std::ios::end);
        auto fsize = f.tellg();
        f.seekg(0, std::ios::beg);
        size_t to_read = std::min<size_t>(fsize, data_.size());
        f.read(reinterpret_cast<char*>(data_.data()), to_read);
        return f.good() || f.eof();
    }

    uint32_t read(uint32_t addr, Width w) override {
        if (addr >= data_.size()) return 0;
        switch (w) {
            case Width::Byte: return data_[addr];
            case Width::Half:
                if (addr + 1 >= data_.size()) return 0;
                return data_[addr] | (data_[addr + 1] << 8);
            case Width::Word:
                if (addr + 3 >= data_.size()) return 0;
                return data_[addr] | (data_[addr + 1] << 8) |
                       (data_[addr + 2] << 16) | (data_[addr + 3] << 24);
        }
        return 0;
    }

    void write(uint32_t, Width, uint32_t) override {
        // ROM is read-only, ignore writes
    }
};

} // namespace cosmo
