// DMA Controller
// CH32V307-style DMA with 8 channels

#pragma once

#include "../bus.hpp"
#include <array>
#include <cstdint>
#include <functional>

namespace cosmo {

// DMA Channel Control Register bits
namespace DMA_CCR {
    constexpr uint32_t EN      = 1 << 0;   // Channel enable
    constexpr uint32_t TCIE    = 1 << 1;   // Transfer complete interrupt enable
    constexpr uint32_t HTIE    = 1 << 2;   // Half transfer interrupt enable
    constexpr uint32_t TEIE    = 1 << 3;   // Transfer error interrupt enable
    constexpr uint32_t DIR     = 1 << 4;   // Direction: 0=P→M, 1=M→P
    constexpr uint32_t CIRC    = 1 << 5;   // Circular mode
    constexpr uint32_t PINC    = 1 << 6;   // Peripheral address increment
    constexpr uint32_t MINC    = 1 << 7;   // Memory address increment
    constexpr uint32_t PSIZE_MASK = 3 << 8;  // Peripheral size
    constexpr uint32_t MSIZE_MASK = 3 << 10; // Memory size
    constexpr uint32_t PL_MASK    = 3 << 12; // Priority level
    constexpr uint32_t MEM2MEM = 1 << 14;  // Memory-to-memory mode
}

// DMA ISR bits per channel (4 bits each)
namespace DMA_ISR {
    constexpr uint32_t GIF  = 1 << 0;  // Global interrupt flag
    constexpr uint32_t TCIF = 1 << 1;  // Transfer complete
    constexpr uint32_t HTIF = 1 << 2;  // Half transfer
    constexpr uint32_t TEIF = 1 << 3;  // Transfer error
}

// DMA IRQ numbers (CH32V307 style)
constexpr int DMA1_CH1_IRQ = 16;
constexpr int DMA1_CH2_IRQ = 17;
constexpr int DMA1_CH3_IRQ = 18;
constexpr int DMA1_CH4_IRQ = 19;
constexpr int DMA1_CH5_IRQ = 20;
constexpr int DMA1_CH6_IRQ = 21;
constexpr int DMA1_CH7_IRQ = 22;
constexpr int DMA1_CH8_IRQ = 23;

struct DMAChannel {
    uint32_t ccr = 0;      // Control register
    uint32_t cndtr = 0;    // Number of data to transfer
    uint32_t cpar = 0;     // Peripheral address
    uint32_t cmar = 0;     // Memory address

    // Runtime state
    uint32_t remaining = 0;     // Remaining transfers
    uint32_t current_par = 0;   // Current peripheral address
    uint32_t current_mar = 0;   // Current memory address
    uint32_t reload_count = 0;  // For circular mode
};

class DMA : public Device {
public:
    static constexpr int NUM_CHANNELS = 8;

    // Callback for bus access (DMA needs to read/write through bus)
    using BusReadFn = std::function<uint32_t(uint32_t addr, Width w)>;
    using BusWriteFn = std::function<void(uint32_t addr, Width w, uint32_t val)>;

    void set_bus_callbacks(BusReadFn read, BusWriteFn write) {
        bus_read_ = std::move(read);
        bus_write_ = std::move(write);
    }

    uint32_t read(uint32_t addr, Width w) override {
        addr &= 0xFFF;

        if (addr == 0x00) return isr_;
        if (addr == 0x04) return 0; // IFCR is write-only

        // Channel registers: base 0x08, stride 0x14
        if (addr >= 0x08) {
            uint32_t offset = addr - 0x08;
            uint32_t ch = offset / 0x14;
            uint32_t reg = offset % 0x14;

            if (ch < NUM_CHANNELS) {
                switch (reg) {
                    case 0x00: return channels_[ch].ccr;
                    case 0x04:
                        // Return remaining if transfer in progress, else cndtr
                        return (channels_[ch].ccr & DMA_CCR::EN)
                            ? channels_[ch].remaining
                            : channels_[ch].cndtr;
                    case 0x08: return channels_[ch].cpar;
                    case 0x0C: return channels_[ch].cmar;
                }
            }
        }

        return 0;
    }

    void write(uint32_t addr, Width w, uint32_t val) override {
        addr &= 0xFFF;

        if (addr == 0x00) return; // ISR is read-only

        if (addr == 0x04) {
            // IFCR: Write 1 to clear flags
            isr_ &= ~val;
            return;
        }

        // Channel registers
        if (addr >= 0x08) {
            uint32_t offset = addr - 0x08;
            uint32_t ch = offset / 0x14;
            uint32_t reg = offset % 0x14;

            if (ch < NUM_CHANNELS) {
                auto& chan = channels_[ch];
                switch (reg) {
                    case 0x00: // CCR
                        {
                            bool was_enabled = chan.ccr & DMA_CCR::EN;
                            bool now_enabled = val & DMA_CCR::EN;
                            chan.ccr = val;

                            // Rising edge of EN: start transfer
                            if (!was_enabled && now_enabled) {
                                start_channel(ch);
                            }
                        }
                        break;
                    case 0x04:
                        chan.cndtr = val & 0xFFFF;
                        chan.reload_count = chan.cndtr;
                        break;
                    case 0x08: chan.cpar = val; break;
                    case 0x0C: chan.cmar = val; break;
                }
            }
        }
    }

    // Tick DMA - called from main loop
    // Returns IRQ number if interrupt pending, -1 otherwise
    std::optional<Interrupt> tick(uint64_t cycles) override {
        // Process one transfer per enabled channel per tick
        // Priority: lower channel number = higher priority
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            auto& chan = channels_[ch];

            if (!(chan.ccr & DMA_CCR::EN)) continue;
            if (chan.remaining == 0) continue;

            do_transfer(ch);

            // Check for completion
            if (chan.remaining == 0) {
                // Set transfer complete flag
                isr_ |= (DMA_ISR::TCIF | DMA_ISR::GIF) << (ch * 4);

                if (chan.ccr & DMA_CCR::CIRC) {
                    // Circular mode: reload
                    chan.remaining = chan.reload_count;
                    chan.current_par = chan.cpar;
                    chan.current_mar = chan.cmar;
                } else {
                    // One-shot: disable channel
                    chan.ccr &= ~DMA_CCR::EN;
                }

                // Generate interrupt if enabled
                if (chan.ccr & DMA_CCR::TCIE) {
                    return Interrupt{static_cast<uint32_t>(DMA1_CH1_IRQ + ch)};
                }
            }

            // Only process one channel per tick for fairness
            break;
        }

        return std::nullopt;
    }

    // Check if any channel has pending interrupt
    bool has_pending_irq() const {
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            uint32_t flags = (isr_ >> (ch * 4)) & 0xF;
            if ((flags & DMA_ISR::TCIF) && (channels_[ch].ccr & DMA_CCR::TCIE)) {
                return true;
            }
        }
        return false;
    }

    // Get channel status for debugging
    uint32_t get_isr() const { return isr_; }

private:
    std::array<DMAChannel, NUM_CHANNELS> channels_{};
    uint32_t isr_ = 0;  // Interrupt status register

    BusReadFn bus_read_;
    BusWriteFn bus_write_;

    void start_channel(int ch) {
        auto& chan = channels_[ch];
        chan.remaining = chan.cndtr;
        chan.reload_count = chan.cndtr;
        chan.current_par = chan.cpar;
        chan.current_mar = chan.cmar;
    }

    void do_transfer(int ch) {
        auto& chan = channels_[ch];

        if (!bus_read_ || !bus_write_) return;

        // Determine transfer size
        uint32_t psize = (chan.ccr & DMA_CCR::PSIZE_MASK) >> 8;
        uint32_t msize = (chan.ccr & DMA_CCR::MSIZE_MASK) >> 10;

        Width pw = (psize == 0) ? Width::Byte : (psize == 1) ? Width::Half : Width::Word;
        Width mw = (msize == 0) ? Width::Byte : (msize == 1) ? Width::Half : Width::Word;

        uint32_t src_addr, dst_addr;
        Width src_width, dst_width;
        bool src_inc, dst_inc;

        if (chan.ccr & DMA_CCR::MEM2MEM) {
            // Memory-to-memory: peripheral addr is actually source memory
            src_addr = chan.current_par;
            dst_addr = chan.current_mar;
            src_width = pw;
            dst_width = mw;
            src_inc = chan.ccr & DMA_CCR::PINC;
            dst_inc = chan.ccr & DMA_CCR::MINC;
        } else if (chan.ccr & DMA_CCR::DIR) {
            // Memory-to-peripheral
            src_addr = chan.current_mar;
            dst_addr = chan.current_par;
            src_width = mw;
            dst_width = pw;
            src_inc = chan.ccr & DMA_CCR::MINC;
            dst_inc = chan.ccr & DMA_CCR::PINC;
        } else {
            // Peripheral-to-memory
            src_addr = chan.current_par;
            dst_addr = chan.current_mar;
            src_width = pw;
            dst_width = mw;
            src_inc = chan.ccr & DMA_CCR::PINC;
            dst_inc = chan.ccr & DMA_CCR::MINC;
        }

        // Do the transfer
        uint32_t data = bus_read_(src_addr, src_width);
        bus_write_(dst_addr, dst_width, data);

        // Update addresses
        uint32_t src_step = (src_width == Width::Byte) ? 1 : (src_width == Width::Half) ? 2 : 4;
        uint32_t dst_step = (dst_width == Width::Byte) ? 1 : (dst_width == Width::Half) ? 2 : 4;

        if (chan.ccr & DMA_CCR::MEM2MEM) {
            if (src_inc) chan.current_par += src_step;
            if (dst_inc) chan.current_mar += dst_step;
        } else if (chan.ccr & DMA_CCR::DIR) {
            if (src_inc) chan.current_mar += src_step;
            if (dst_inc) chan.current_par += dst_step;
        } else {
            if (src_inc) chan.current_par += src_step;
            if (dst_inc) chan.current_mar += dst_step;
        }

        chan.remaining--;
    }
};

} // namespace cosmo
