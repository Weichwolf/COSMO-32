#pragma once
#include "../bus.hpp"
#include "pfic.hpp"
#include <cstdio>
#include <deque>
#include <functional>

namespace cosmo {

// USART with TX and RX support
// Register layout (mimics CH32V307 USART):
//   0x00 STATR  - Status Register (ro)
//   0x04 DATAR  - Data Register (rw)
//   0x08 BRR    - Baud Rate Register (rw, ignored)
//   0x0C CTLR1  - Control Register 1 (rw)
//   0x10 CTLR2  - Control Register 2 (rw, ignored)
//   0x14 CTLR3  - Control Register 3 (rw, ignored)
//   0x18 GPR    - Guard time and prescaler (rw, ignored)

class USART : public Device {
public:
    // Status register bits
    static constexpr uint32_t STATR_TXE  = 1 << 7;  // TX empty
    static constexpr uint32_t STATR_TC   = 1 << 6;  // Transmission complete
    static constexpr uint32_t STATR_RXNE = 1 << 5;  // RX not empty

    // Control register 1 bits
    static constexpr uint32_t CTLR1_UE     = 1 << 13; // USART enable
    static constexpr uint32_t CTLR1_RXNEIE = 1 << 5;  // RXNE interrupt enable
    static constexpr uint32_t CTLR1_TE     = 1 << 3;  // TX enable
    static constexpr uint32_t CTLR1_RE     = 1 << 2;  // RX enable

    // Default IRQ number for USART1 (CH32V307)
    static constexpr uint32_t DEFAULT_IRQ = 37;

    // RX queue limit (matches shell buffer size in OS)
    static constexpr size_t RX_QUEUE_MAX = 4096;

    using OutputCallback = std::function<void(char)>;

private:
    uint32_t brr_ = 0;
    uint32_t ctlr1_ = 0;
    uint32_t ctlr2_ = 0;
    uint32_t ctlr3_ = 0;
    uint32_t gpr_ = 0;

    std::deque<uint8_t> rx_queue_;
    OutputCallback output_cb_;
    PFIC* pfic_ = nullptr;
    uint32_t irq_num_ = DEFAULT_IRQ;

    void update_irq();

public:
    USART() : output_cb_([](char c) { std::putchar(c); std::fflush(stdout); }) {}

    void set_pfic(PFIC* pfic, uint32_t irq = DEFAULT_IRQ) {
        pfic_ = pfic;
        irq_num_ = irq;
    }

    // Queue input byte (called from host)
    void queue_input(uint8_t byte) {
        if (rx_queue_.size() < RX_QUEUE_MAX) {
            rx_queue_.push_back(byte);
            update_irq();
        }
    }

    // Queue string (convenience), respects queue limit
    void queue_input(const char* str) {
        while (*str && rx_queue_.size() < RX_QUEUE_MAX) {
            rx_queue_.push_back(static_cast<uint8_t>(*str++));
        }
        update_irq();
    }

    bool has_input() const { return !rx_queue_.empty(); }

    void set_output_callback(OutputCallback cb) {
        output_cb_ = std::move(cb);
    }

    uint32_t read(uint32_t addr, Width) override {
        switch (addr) {
            case 0x00: {
                // STATR - build status dynamically
                uint32_t statr = STATR_TXE | STATR_TC;  // TX always ready
                if (!rx_queue_.empty()) {
                    statr |= STATR_RXNE;
                }
                return statr;
            }
            case 0x04: {
                // DATAR - read clears RXNE (and potentially IRQ)
                if (!rx_queue_.empty()) {
                    uint8_t byte = rx_queue_.front();
                    rx_queue_.pop_front();
                    // Clear IRQ if queue now empty
                    if (rx_queue_.empty() && pfic_) {
                        pfic_->clear_pending(irq_num_);
                    }
                    return byte;
                }
                return 0;
            }
            case 0x08: return brr_;
            case 0x0C: return ctlr1_;
            case 0x10: return ctlr2_;
            case 0x14: return ctlr3_;
            case 0x18: return gpr_;
            default:   return 0;
        }
    }

    void write(uint32_t addr, Width, uint32_t val) override {
        switch (addr) {
            case 0x00: // STATR - read only, but writing clears some bits
                break;
            case 0x04: // DATAR - write transmits
                if ((ctlr1_ & CTLR1_UE) && (ctlr1_ & CTLR1_TE)) {
                    if (output_cb_) {
                        output_cb_(static_cast<char>(val & 0xFF));
                    }
                }
                break;
            case 0x08: brr_ = val; break;
            case 0x0C:
                ctlr1_ = val;
                update_irq();  // Re-evaluate IRQ when RXNEIE changes
                break;
            case 0x10: ctlr2_ = val; break;
            case 0x14: ctlr3_ = val; break;
            case 0x18: gpr_ = val; break;
        }
    }

    bool is_enabled() const { return ctlr1_ & CTLR1_UE; }
    bool is_tx_enabled() const { return ctlr1_ & CTLR1_TE; }
    bool is_rxne_irq_enabled() const { return ctlr1_ & CTLR1_RXNEIE; }
};

inline void USART::update_irq() {
    if (!pfic_) return;

    // Set IRQ pending if RXNE and RXNEIE both set
    if (!rx_queue_.empty() && (ctlr1_ & CTLR1_RXNEIE)) {
        pfic_->set_pending(irq_num_);
    }
}

} // namespace cosmo
