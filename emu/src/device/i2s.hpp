// I2S Audio Peripheral
// Stereo 16-bit audio output with DMA support

#pragma once

#include "../bus.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace cosmo {

// I2S Register offsets
namespace I2S_Reg {
    constexpr uint32_t CTRL   = 0x00;  // Control register
    constexpr uint32_t STATUS = 0x04;  // Status register
    constexpr uint32_t DATA   = 0x08;  // Data register (write samples here)
    constexpr uint32_t CLKDIV = 0x0C;  // Clock divider (sample rate)
    constexpr uint32_t BUFCNT = 0x10;  // Buffer count (samples in buffer)
}

// Control register bits
namespace I2S_CTRL {
    constexpr uint32_t EN     = 1 << 0;   // Enable
    constexpr uint32_t TXIE   = 1 << 1;   // TX empty interrupt enable
    constexpr uint32_t DMAE   = 1 << 2;   // DMA enable
    constexpr uint32_t STEREO = 1 << 3;   // 0=mono, 1=stereo
    constexpr uint32_t FMT16  = 1 << 4;   // 0=8bit, 1=16bit
}

// Status register bits
namespace I2S_STATUS {
    constexpr uint32_t TXE    = 1 << 0;   // TX buffer empty
    constexpr uint32_t TXNF   = 1 << 1;   // TX buffer not full
    constexpr uint32_t TXHF   = 1 << 2;   // TX buffer half full
    constexpr uint32_t BSY    = 1 << 3;   // Busy (playing)
}

// I2S IRQ number
constexpr uint32_t I2S_IRQ = 25;

// DMA channel for I2S (convention)
constexpr uint32_t I2S_DMA_CH = 3;

class I2S : public Device {
public:
    // Buffer size in stereo samples (L+R = 1 sample)
    static constexpr size_t BUFFER_SIZE = 2048;
    static constexpr size_t HALF_BUFFER = BUFFER_SIZE / 2;

    // Default sample rate
    static constexpr uint32_t DEFAULT_SAMPLE_RATE = 22050;
    static constexpr uint32_t CPU_CLOCK = 144'000'000;

    I2S() : buffer_(BUFFER_SIZE * 2) {  // *2 for L+R channels
        // Default clock divider for 22050 Hz
        clkdiv_ = CPU_CLOCK / DEFAULT_SAMPLE_RATE;
    }

    uint32_t read(uint32_t addr, Width w) override {
        addr &= 0xFF;

        switch (addr) {
            case I2S_Reg::CTRL:
                return ctrl_;

            case I2S_Reg::STATUS:
                return get_status();

            case I2S_Reg::DATA:
                return 0;  // Write-only

            case I2S_Reg::CLKDIV:
                return clkdiv_;

            case I2S_Reg::BUFCNT:
                return sample_count_;
        }

        return 0;
    }

    void write(uint32_t addr, Width w, uint32_t val) override {
        addr &= 0xFF;

        switch (addr) {
            case I2S_Reg::CTRL:
                ctrl_ = val;
                if (!(ctrl_ & I2S_CTRL::EN)) {
                    // Reset on disable
                    write_pos_ = 0;
                    read_pos_ = 0;
                    sample_count_ = 0;
                }
                break;

            case I2S_Reg::STATUS:
                // Read-only (or write-1-to-clear for flags)
                break;

            case I2S_Reg::DATA:
                write_sample(val);
                break;

            case I2S_Reg::CLKDIV:
                clkdiv_ = val;
                break;
        }
    }

    // Tick - consume samples at sample rate
    std::optional<Interrupt> tick(uint64_t cycles) override {
        if (!(ctrl_ & I2S_CTRL::EN)) return std::nullopt;

        // Calculate cycles per sample
        uint64_t cycles_per_sample = clkdiv_;
        if (cycles_per_sample == 0) cycles_per_sample = 1;

        // Check if it's time to consume a sample
        if (cycles - last_sample_cycle_ >= cycles_per_sample) {
            last_sample_cycle_ = cycles;

            if (sample_count_ > 0) {
                // Consume one stereo sample (2 values)
                read_pos_ = (read_pos_ + 2) % buffer_.size();
                sample_count_--;
            }

            // Generate interrupt if buffer below threshold and interrupts enabled
            if ((ctrl_ & I2S_CTRL::TXIE) && sample_count_ < HALF_BUFFER) {
                return Interrupt{I2S_IRQ};
            }
        }

        return std::nullopt;
    }

    // Get sample rate from clock divider
    uint32_t sample_rate() const {
        return clkdiv_ > 0 ? CPU_CLOCK / clkdiv_ : DEFAULT_SAMPLE_RATE;
    }

    // Check if DMA request should be asserted
    bool dma_request() const {
        return (ctrl_ & I2S_CTRL::EN) &&
               (ctrl_ & I2S_CTRL::DMAE) &&
               (sample_count_ < HALF_BUFFER);
    }

    // Read samples for audio output (called by SDL callback later)
    size_t read_samples(int16_t* out, size_t count) {
        size_t read = 0;
        while (read < count && sample_count_ > 0) {
            out[read * 2] = buffer_[read_pos_];
            out[read * 2 + 1] = buffer_[read_pos_ + 1];
            read_pos_ = (read_pos_ + 2) % buffer_.size();
            sample_count_--;
            read++;
        }
        return read;
    }

    // Direct buffer access for testing
    size_t buffer_count() const { return sample_count_; }
    bool is_enabled() const { return ctrl_ & I2S_CTRL::EN; }

private:
    uint32_t ctrl_ = 0;
    uint32_t clkdiv_ = CPU_CLOCK / DEFAULT_SAMPLE_RATE;
    std::vector<int16_t> buffer_;
    size_t write_pos_ = 0;
    size_t read_pos_ = 0;
    size_t sample_count_ = 0;
    uint64_t last_sample_cycle_ = 0;

    uint32_t get_status() const {
        uint32_t status = 0;

        if (sample_count_ == 0) {
            status |= I2S_STATUS::TXE;
        }
        if (sample_count_ < BUFFER_SIZE) {
            status |= I2S_STATUS::TXNF;
        }
        if (sample_count_ >= HALF_BUFFER) {
            status |= I2S_STATUS::TXHF;
        }
        if ((ctrl_ & I2S_CTRL::EN) && sample_count_ > 0) {
            status |= I2S_STATUS::BSY;
        }

        return status;
    }

    void write_sample(uint32_t val) {
        if (!(ctrl_ & I2S_CTRL::EN)) return;
        if (sample_count_ >= BUFFER_SIZE) return;  // Buffer full

        if (ctrl_ & I2S_CTRL::STEREO) {
            // Stereo: val = (right << 16) | left
            buffer_[write_pos_] = static_cast<int16_t>(val & 0xFFFF);
            buffer_[write_pos_ + 1] = static_cast<int16_t>((val >> 16) & 0xFFFF);
        } else {
            // Mono: duplicate to both channels
            int16_t sample = static_cast<int16_t>(val & 0xFFFF);
            buffer_[write_pos_] = sample;
            buffer_[write_pos_ + 1] = sample;
        }

        write_pos_ = (write_pos_ + 2) % buffer_.size();
        sample_count_++;
    }
};

} // namespace cosmo
