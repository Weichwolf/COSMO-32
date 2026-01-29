// Ethernet MAC Peripheral
// Simplified 10M Ethernet with DMA descriptors and built-in services:
// - UDP Echo (port 7)
// - ICMP Echo (ping)
// - DHCP Server (ports 67/68)
// - TFTP Server (port 69)

#pragma once

#include "../bus.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>
#include <deque>
#include <map>
#include <string>
#include <cassert>

namespace cosmo {

// Safe packet builder with bounds checking
class PacketBuilder {
    std::vector<uint8_t>& pkt_;
public:
    explicit PacketBuilder(std::vector<uint8_t>& pkt) : pkt_(pkt) {}

    void write_u8(size_t off, uint8_t val) {
        assert(off < pkt_.size());
        pkt_[off] = val;
    }

    void write_u16_be(size_t off, uint16_t val) {
        assert(off + 1 < pkt_.size());
        pkt_[off] = (val >> 8) & 0xFF;
        pkt_[off + 1] = val & 0xFF;
    }

    void write_u32_be(size_t off, uint32_t val) {
        assert(off + 3 < pkt_.size());
        pkt_[off] = (val >> 24) & 0xFF;
        pkt_[off + 1] = (val >> 16) & 0xFF;
        pkt_[off + 2] = (val >> 8) & 0xFF;
        pkt_[off + 3] = val & 0xFF;
    }

    void write_bytes(size_t off, const uint8_t* src, size_t len) {
        assert(off + len <= pkt_.size());
        std::memcpy(&pkt_[off], src, len);
    }

    size_t size() const { return pkt_.size(); }
    uint8_t& operator[](size_t i) { assert(i < pkt_.size()); return pkt_[i]; }
};

// ETH Register offsets
namespace ETH_Reg {
    constexpr uint32_t MACCR    = 0x00;   // MAC Configuration
    constexpr uint32_t MACSR    = 0x04;   // MAC Status (reserved)
    constexpr uint32_t MACA0HR  = 0x08;   // MAC Address 0 High
    constexpr uint32_t MACA0LR  = 0x0C;   // MAC Address 0 Low
    constexpr uint32_t DMAOMR   = 0x10;   // DMA Operation Mode
    constexpr uint32_t DMASR    = 0x14;   // DMA Status
    constexpr uint32_t DMATDLAR = 0x18;   // TX Descriptor List Address
    constexpr uint32_t DMARDLAR = 0x1C;   // RX Descriptor List Address
    constexpr uint32_t DMATPDR  = 0x20;   // TX Poll Demand
    constexpr uint32_t DMARPDR  = 0x24;   // RX Poll Demand
    constexpr uint32_t DMACHTDR = 0x28;   // Current TX Descriptor
    constexpr uint32_t DMACHRDR = 0x2C;   // Current RX Descriptor
}

// MACCR bits
namespace ETH_MACCR {
    constexpr uint32_t TE = 1 << 0;   // Transmitter Enable
    constexpr uint32_t RE = 1 << 1;   // Receiver Enable
}

// DMAOMR bits
namespace ETH_DMAOMR {
    constexpr uint32_t SR = 1 << 0;   // Start/Stop Receive
    constexpr uint32_t ST = 1 << 1;   // Start/Stop Transmit
}

// DMASR bits
namespace ETH_DMASR {
    constexpr uint32_t TS  = 1 << 0;  // Transmit Status
    constexpr uint32_t RS  = 1 << 1;  // Receive Status
    constexpr uint32_t TU  = 1 << 2;  // Transmit Buffer Unavailable
    constexpr uint32_t RU  = 1 << 3;  // Receive Buffer Unavailable
    constexpr uint32_t NIS = 1 << 4;  // Normal Interrupt Summary
    constexpr uint32_t AIS = 1 << 5;  // Abnormal Interrupt Summary
}

// TX Descriptor Status bits (TDES0)
namespace ETH_TDES0 {
    constexpr uint32_t OWN = 1u << 31;  // Owned by DMA
    constexpr uint32_t IC  = 1 << 30;   // Interrupt on Completion
    constexpr uint32_t LS  = 1 << 29;   // Last Segment
    constexpr uint32_t FS  = 1 << 28;   // First Segment
    constexpr uint32_t TCH = 1 << 20;   // Second Address Chained
}

// RX Descriptor Status bits (RDES0)
namespace ETH_RDES0 {
    constexpr uint32_t OWN = 1u << 31;  // Owned by DMA
    constexpr uint32_t FL_MASK = 0x3FFF << 16;  // Frame Length
    constexpr uint32_t FL_SHIFT = 16;
    constexpr uint32_t LS  = 1 << 9;    // Last Descriptor
    constexpr uint32_t FS  = 1 << 8;    // First Descriptor
}

// RDES1 bits
namespace ETH_RDES1 {
    constexpr uint32_t RCH = 1 << 14;   // Second Address Chained
    constexpr uint32_t RBS1_MASK = 0x1FFF;  // Buffer 1 Size
}

// ETH IRQ number
constexpr uint32_t ETH_IRQ = 26;

// Protocol constants
constexpr uint16_t ETHERTYPE_IP = 0x0800;
constexpr uint8_t IP_PROTO_ICMP = 1;
constexpr uint8_t IP_PROTO_UDP = 17;

// UDP ports
constexpr uint16_t UDP_ECHO_PORT = 7;
constexpr uint16_t TFTP_PORT = 69;
constexpr uint16_t DHCP_SERVER_PORT = 67;
constexpr uint16_t DHCP_CLIENT_PORT = 68;

// TFTP opcodes
constexpr uint16_t TFTP_RRQ   = 1;   // Read Request
constexpr uint16_t TFTP_WRQ   = 2;   // Write Request
constexpr uint16_t TFTP_DATA  = 3;   // Data
constexpr uint16_t TFTP_ACK   = 4;   // Acknowledgment
constexpr uint16_t TFTP_ERROR = 5;   // Error

// TFTP error codes
constexpr uint16_t TFTP_ERR_NOT_FOUND   = 1;   // File not found
constexpr uint16_t TFTP_ERR_ACCESS      = 2;   // Access violation
constexpr uint16_t TFTP_ERR_DISK_FULL   = 3;   // Disk full
constexpr uint16_t TFTP_ERR_ILLEGAL_OP  = 4;   // Illegal operation
constexpr uint16_t TFTP_ERR_UNKNOWN_TID = 5;   // Unknown transfer ID

// ICMP types
constexpr uint8_t ICMP_ECHO_REQUEST = 8;
constexpr uint8_t ICMP_ECHO_REPLY = 0;

// DHCP message types
constexpr uint8_t DHCP_DISCOVER = 1;
constexpr uint8_t DHCP_OFFER = 2;
constexpr uint8_t DHCP_REQUEST = 3;
constexpr uint8_t DHCP_ACK = 5;

// DHCP options
constexpr uint8_t DHCP_OPT_SUBNET = 1;
constexpr uint8_t DHCP_OPT_ROUTER = 3;
constexpr uint8_t DHCP_OPT_DNS = 6;
constexpr uint8_t DHCP_OPT_LEASE = 51;
constexpr uint8_t DHCP_OPT_MSGTYPE = 53;
constexpr uint8_t DHCP_OPT_SERVER = 54;
constexpr uint8_t DHCP_OPT_END = 255;

// Network configuration (emulator virtual network)
constexpr uint8_t EMU_SERVER_IP[4] = {10, 0, 0, 1};
constexpr uint8_t EMU_CLIENT_IP[4] = {10, 0, 0, 2};
constexpr uint8_t EMU_SUBNET[4] = {255, 255, 255, 0};
constexpr uint8_t EMU_SERVER_MAC[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};

// TFTP session state for active transfers
struct TftpSession {
    uint16_t client_port;          // Client's ephemeral port (TID)
    uint8_t client_ip[4];          // Client IP
    uint8_t client_mac[6];         // Client MAC
    bool is_read;                  // true = RRQ (server sending), false = WRQ (server receiving)
    uint16_t block_num;            // Current block number
    std::vector<uint8_t> file_data;// File content (for RRQ) or accumulator (for WRQ)
    std::string filename;          // File being transferred
    size_t offset;                 // Current offset in file_data (for RRQ)
};

class ETH : public Device {
public:
    // Bus callbacks for DMA descriptor access
    using BusReadFn = std::function<uint32_t(uint32_t addr, Width w)>;
    using BusWriteFn = std::function<void(uint32_t addr, Width w, uint32_t val)>;

    void set_bus_callbacks(BusReadFn read, BusWriteFn write) {
        bus_read_ = std::move(read);
        bus_write_ = std::move(write);
    }

    // Set TFTP root directory
    void set_tftp_root(const std::string& path) {
        tftp_root_ = path;
    }

    uint32_t read(uint32_t addr, Width w) override {
        addr &= 0xFFF;

        switch (addr) {
            case ETH_Reg::MACCR:    return maccr_;
            case ETH_Reg::MACSR:    return 0;
            case ETH_Reg::MACA0HR:  return mac_addr_high_;
            case ETH_Reg::MACA0LR:  return mac_addr_low_;
            case ETH_Reg::DMAOMR:   return dmaomr_;
            case ETH_Reg::DMASR:    return dmasr_;
            case ETH_Reg::DMATDLAR: return tx_desc_list_;
            case ETH_Reg::DMARDLAR: return rx_desc_list_;
            case ETH_Reg::DMATPDR:  return 0;
            case ETH_Reg::DMARPDR:  return 0;
            case ETH_Reg::DMACHTDR: return current_tx_desc_;
            case ETH_Reg::DMACHRDR: return current_rx_desc_;
        }
        return 0;
    }

    void write(uint32_t addr, Width w, uint32_t val) override {
        addr &= 0xFFF;

        switch (addr) {
            case ETH_Reg::MACCR:
                maccr_ = val;
                break;
            case ETH_Reg::MACA0HR:
                mac_addr_high_ = val & 0xFFFF;
                break;
            case ETH_Reg::MACA0LR:
                mac_addr_low_ = val;
                break;
            case ETH_Reg::DMAOMR:
                dmaomr_ = val;
                break;
            case ETH_Reg::DMASR:
                // Write 1 to clear status bits
                dmasr_ &= ~(val & 0x3F);
                break;
            case ETH_Reg::DMATDLAR:
                tx_desc_list_ = val;
                current_tx_desc_ = val;
                break;
            case ETH_Reg::DMARDLAR:
                rx_desc_list_ = val;
                current_rx_desc_ = val;
                break;
            case ETH_Reg::DMATPDR:
                // TX Poll Demand - trigger TX processing
                tx_poll_pending_ = true;
                break;
            case ETH_Reg::DMARPDR:
                // RX Poll Demand - trigger RX processing
                rx_poll_pending_ = true;
                break;
        }
    }

    std::optional<Interrupt> tick(uint64_t cycles) override {
        if (!bus_read_ || !bus_write_) {
            return std::nullopt;
        }

        bool irq_pending = false;

        // Process TX if enabled and poll pending
        if ((maccr_ & ETH_MACCR::TE) && (dmaomr_ & ETH_DMAOMR::ST) && tx_poll_pending_) {
            irq_pending |= process_tx();
            tx_poll_pending_ = false;
        }

        // Process RX if enabled - deliver any pending frames
        if ((maccr_ & ETH_MACCR::RE) && (dmaomr_ & ETH_DMAOMR::SR)) {
            irq_pending |= process_rx();
        }

        if (irq_pending) {
            return Interrupt{ETH_IRQ};
        }
        return std::nullopt;
    }

    // Get MAC address as bytes
    void get_mac_address(uint8_t* out) const {
        out[0] = (mac_addr_high_ >> 8) & 0xFF;
        out[1] = mac_addr_high_ & 0xFF;
        out[2] = (mac_addr_low_ >> 24) & 0xFF;
        out[3] = (mac_addr_low_ >> 16) & 0xFF;
        out[4] = (mac_addr_low_ >> 8) & 0xFF;
        out[5] = mac_addr_low_ & 0xFF;
    }

private:
    uint32_t maccr_ = 0;
    uint32_t mac_addr_high_ = 0x0002;  // Default: 00:02:xx:xx:xx:xx
    uint32_t mac_addr_low_ = 0x03040506;
    uint32_t dmaomr_ = 0;
    uint32_t dmasr_ = 0;
    uint32_t tx_desc_list_ = 0;
    uint32_t rx_desc_list_ = 0;
    uint32_t current_tx_desc_ = 0;
    uint32_t current_rx_desc_ = 0;
    bool tx_poll_pending_ = false;
    bool rx_poll_pending_ = false;

    BusReadFn bus_read_;
    BusWriteFn bus_write_;

    // Pending RX frames
    std::deque<std::vector<uint8_t>> rx_queue_;

    // TFTP state
    std::string tftp_root_;
    std::map<uint16_t, TftpSession> tftp_sessions_;  // keyed by client port

    bool process_tx() {
        if (current_tx_desc_ == 0) return false;

        // Read TX descriptor
        uint32_t tdes0 = bus_read_(current_tx_desc_ + 0, Width::Word);
        uint32_t tdes1 = bus_read_(current_tx_desc_ + 4, Width::Word);
        uint32_t tdes2 = bus_read_(current_tx_desc_ + 8, Width::Word);
        uint32_t tdes3 = bus_read_(current_tx_desc_ + 12, Width::Word);

        // Check if owned by DMA
        if (!(tdes0 & ETH_TDES0::OWN)) {
            dmasr_ |= ETH_DMASR::TU;
            return false;
        }

        // Get buffer size and address
        uint32_t buf_size = tdes1 & 0x1FFF;
        uint32_t buf_addr = tdes2;

        // Read frame data
        std::vector<uint8_t> frame(buf_size);
        for (uint32_t i = 0; i < buf_size; i++) {
            frame[i] = bus_read_(buf_addr + i, Width::Byte) & 0xFF;
        }

        // Process the frame (UDP echo)
        process_frame(frame);

        // Clear OWN bit, set status
        tdes0 &= ~ETH_TDES0::OWN;
        bus_write_(current_tx_desc_ + 0, Width::Word, tdes0);

        // Move to next descriptor
        if (tdes0 & ETH_TDES0::TCH) {
            current_tx_desc_ = tdes3;
        } else {
            current_tx_desc_ += 16;
        }

        // Set TX complete status
        dmasr_ |= ETH_DMASR::TS | ETH_DMASR::NIS;
        return (tdes0 & ETH_TDES0::IC) != 0;
    }

    bool process_rx() {
        if (rx_queue_.empty()) return false;
        if (current_rx_desc_ == 0) return false;

        // Read RX descriptor
        uint32_t rdes0 = bus_read_(current_rx_desc_ + 0, Width::Word);
        uint32_t rdes1 = bus_read_(current_rx_desc_ + 4, Width::Word);
        uint32_t rdes2 = bus_read_(current_rx_desc_ + 8, Width::Word);
        uint32_t rdes3 = bus_read_(current_rx_desc_ + 12, Width::Word);

        // Check if owned by DMA
        if (!(rdes0 & ETH_RDES0::OWN)) {
            dmasr_ |= ETH_DMASR::RU;
            return false;
        }

        // Get buffer size and address
        uint32_t buf_size = rdes1 & ETH_RDES1::RBS1_MASK;
        uint32_t buf_addr = rdes2;

        // Get frame from queue
        auto& frame = rx_queue_.front();
        uint32_t frame_len = std::min(static_cast<uint32_t>(frame.size()), buf_size);

        // Write frame to buffer
        for (uint32_t i = 0; i < frame_len; i++) {
            bus_write_(buf_addr + i, Width::Byte, frame[i]);
        }

        rx_queue_.pop_front();

        // Update descriptor
        rdes0 &= ~ETH_RDES0::OWN;
        rdes0 |= ETH_RDES0::FS | ETH_RDES0::LS;
        rdes0 = (rdes0 & ~ETH_RDES0::FL_MASK) | (frame_len << ETH_RDES0::FL_SHIFT);
        bus_write_(current_rx_desc_ + 0, Width::Word, rdes0);

        // Move to next descriptor
        if (rdes1 & ETH_RDES1::RCH) {
            current_rx_desc_ = rdes3;
        } else {
            current_rx_desc_ += 16;
        }

        // Set RX complete status
        dmasr_ |= ETH_DMASR::RS | ETH_DMASR::NIS;
        return true;
    }

    void process_frame(const std::vector<uint8_t>& frame) {
        // Minimum Ethernet frame: 14 (ETH) + 20 (IP) = 34 bytes
        if (frame.size() < 34) return;

        // Check EtherType (IP = 0x0800)
        uint16_t ethertype = (frame[12] << 8) | frame[13];
        if (ethertype != ETHERTYPE_IP) return;

        // Get IP protocol
        uint8_t ip_proto = frame[23];

        if (ip_proto == IP_PROTO_ICMP) {
            process_icmp(frame);
        } else if (ip_proto == IP_PROTO_UDP) {
            process_udp(frame);
        }
    }

    void process_icmp(const std::vector<uint8_t>& frame) {
        // Minimum ICMP: 14 (ETH) + 20 (IP) + 8 (ICMP header) = 42 bytes
        if (frame.size() < 42) return;

        // ICMP starts at offset 34 (after IP header, assuming no options)
        size_t icmp_offset = 14 + ((frame[14] & 0x0F) * 4);
        if (frame.size() < icmp_offset + 8) return;

        uint8_t icmp_type = frame[icmp_offset];
        uint8_t icmp_code = frame[icmp_offset + 1];

        // Only handle Echo Request (type 8, code 0)
        if (icmp_type != ICMP_ECHO_REQUEST || icmp_code != 0) return;

        // Build Echo Reply
        std::vector<uint8_t> response = frame;

        // Swap MAC addresses
        std::swap_ranges(response.begin(), response.begin() + 6,
                        response.begin() + 6);

        // Swap IP addresses
        std::swap_ranges(response.begin() + 26, response.begin() + 30,
                        response.begin() + 30);

        // Change ICMP type to Echo Reply (0)
        response[icmp_offset] = ICMP_ECHO_REPLY;

        // Recalculate ICMP checksum
        recalc_icmp_checksum(response, icmp_offset);

        // Recalculate IP header checksum
        recalc_ip_checksum(response);

        rx_queue_.push_back(std::move(response));
    }

    void process_udp(const std::vector<uint8_t>& frame) {
        // Minimum UDP: 14 (ETH) + 20 (IP) + 8 (UDP) = 42 bytes
        if (frame.size() < 42) return;

        // Get UDP src and dest ports
        uint16_t src_port = (frame[34] << 8) | frame[35];
        uint16_t dst_port = (frame[36] << 8) | frame[37];

        if (dst_port == UDP_ECHO_PORT) {
            process_udp_echo(frame);
        } else if (dst_port == DHCP_SERVER_PORT) {
            process_dhcp(frame);
        } else if (dst_port == TFTP_PORT) {
            // Check if this is continuation of existing TFTP session
            auto it = tftp_sessions_.find(src_port);
            if (it != tftp_sessions_.end()) {
                process_tftp_data(frame, it->second);
            } else {
                process_tftp_initial(frame, src_port);
            }
        } else {
            // Check for TFTP data transfer (client sending to our ephemeral port)
            auto it = tftp_sessions_.find(src_port);
            if (it != tftp_sessions_.end()) {
                process_tftp_data(frame, it->second);
            }
        }
    }

    void process_udp_echo(const std::vector<uint8_t>& frame) {
        // Build echo response
        std::vector<uint8_t> response = frame;

        // Swap MAC addresses
        std::swap_ranges(response.begin(), response.begin() + 6,
                        response.begin() + 6);

        // Swap IP addresses
        std::swap_ranges(response.begin() + 26, response.begin() + 30,
                        response.begin() + 30);

        // Swap UDP ports
        std::swap_ranges(response.begin() + 34, response.begin() + 36,
                        response.begin() + 36);

        // Clear UDP checksum (optional for IPv4)
        response[40] = 0;
        response[41] = 0;

        // Recalculate IP header checksum
        recalc_ip_checksum(response);

        rx_queue_.push_back(std::move(response));
    }

    void process_dhcp(const std::vector<uint8_t>& frame) {
        // DHCP minimum: 14 (ETH) + 20 (IP) + 8 (UDP) + 236 (BOOTP) + 4 (magic) = 278 bytes
        if (frame.size() < 278) return;

        // DHCP payload starts at offset 42 (after UDP header)
        size_t dhcp_offset = 42;

        // Check BOOTP op (1 = request)
        if (frame[dhcp_offset] != 1) return;

        // Get transaction ID (xid)
        uint32_t xid = (frame[dhcp_offset + 4] << 24) |
                       (frame[dhcp_offset + 5] << 16) |
                       (frame[dhcp_offset + 6] << 8) |
                       frame[dhcp_offset + 7];

        // Get client MAC (at offset 28 in DHCP)
        uint8_t client_mac[6];
        for (int i = 0; i < 6; i++) {
            client_mac[i] = frame[dhcp_offset + 28 + i];
        }

        // BOOTP header is 236 bytes, magic cookie at offset 236
        size_t opt_offset = dhcp_offset + 236;
        uint8_t msg_type = 0;

        // Skip magic cookie (4 bytes)
        if (frame.size() < opt_offset + 4) return;
        opt_offset += 4;

        // Parse options to find message type
        while (opt_offset < frame.size() && frame[opt_offset] != DHCP_OPT_END) {
            uint8_t opt = frame[opt_offset++];
            if (opt == 0) continue;  // Pad option
            if (opt_offset >= frame.size()) break;
            uint8_t len = frame[opt_offset++];
            if (opt == DHCP_OPT_MSGTYPE && len >= 1 && opt_offset < frame.size()) {
                msg_type = frame[opt_offset];
            }
            opt_offset += len;
        }

        // Build DHCP response based on message type
        if (msg_type == DHCP_DISCOVER) {
            send_dhcp_offer(xid, client_mac);
        } else if (msg_type == DHCP_REQUEST) {
            send_dhcp_ack(xid, client_mac);
        }
    }

    void send_dhcp_offer(uint32_t xid, const uint8_t* client_mac) {
        send_dhcp_response(xid, client_mac, DHCP_OFFER);
    }

    void send_dhcp_ack(uint32_t xid, const uint8_t* client_mac) {
        send_dhcp_response(xid, client_mac, DHCP_ACK);
    }

    void send_dhcp_response(uint32_t xid, const uint8_t* client_mac, uint8_t msg_type) {
        // Build DHCP response packet with bounds checking
        // ETH (14) + IP (20) + UDP (8) + BOOTP (240) + options (~32) = ~314 bytes
        std::vector<uint8_t> pkt(314, 0);
        PacketBuilder pb(pkt);

        // Ethernet header (0-13)
        pb.write_bytes(0, client_mac, 6);                 // Dest MAC = client
        pb.write_bytes(6, EMU_SERVER_MAC, 6);             // Src MAC = server
        pb.write_u16_be(12, ETHERTYPE_IP);                // EtherType = IP

        // IP header (14-33)
        pb.write_u8(14, 0x45);                            // Version + IHL
        pb.write_u8(15, 0x00);                            // DSCP
        uint16_t ip_len = 20 + 8 + 240 + 32;
        pb.write_u16_be(16, ip_len);                      // Total length
        pb.write_u16_be(18, 0);                           // ID
        pb.write_u16_be(20, 0);                           // Flags + Fragment
        pb.write_u8(22, 64);                              // TTL
        pb.write_u8(23, IP_PROTO_UDP);                    // Protocol
        pb.write_u16_be(24, 0);                           // Checksum (calc later)
        pb.write_bytes(26, EMU_SERVER_IP, 4);             // Src IP
        pb.write_bytes(30, EMU_CLIENT_IP, 4);             // Dest IP

        // UDP header (34-41)
        pb.write_u16_be(34, DHCP_CLIENT_PORT);            // Src port
        pb.write_u16_be(36, DHCP_CLIENT_PORT);            // Dest port
        uint16_t udp_len = 8 + 240 + 32;
        pb.write_u16_be(38, udp_len);                     // Length
        pb.write_u16_be(40, 0);                           // Checksum (optional)

        // BOOTP/DHCP payload (starts at offset 42)
        constexpr size_t d = 42;
        pb.write_u8(d + 0, 2);                            // op = BOOTREPLY
        pb.write_u8(d + 1, 1);                            // htype = Ethernet
        pb.write_u8(d + 2, 6);                            // hlen = 6
        pb.write_u8(d + 3, 0);                            // hops
        pb.write_u32_be(d + 4, xid);                      // xid
        // secs, flags, ciaddr = 0
        pb.write_bytes(d + 16, EMU_CLIENT_IP, 4);         // yiaddr = offered IP
        pb.write_bytes(d + 20, EMU_SERVER_IP, 4);         // siaddr = server IP
        // giaddr = 0
        pb.write_bytes(d + 28, client_mac, 6);            // chaddr = client MAC

        // DHCP magic cookie at offset 236
        pb.write_u8(d + 236, 99);
        pb.write_u8(d + 237, 130);
        pb.write_u8(d + 238, 83);
        pb.write_u8(d + 239, 99);

        // DHCP options start at offset 240 (from BOOTP start)
        size_t o = d + 240;

        // Option 53: DHCP Message Type
        pb.write_u8(o++, DHCP_OPT_MSGTYPE);
        pb.write_u8(o++, 1);
        pb.write_u8(o++, msg_type);

        // Option 54: Server Identifier
        pb.write_u8(o++, DHCP_OPT_SERVER);
        pb.write_u8(o++, 4);
        pb.write_bytes(o, EMU_SERVER_IP, 4);
        o += 4;

        // Option 51: Lease Time (1 hour = 3600 seconds)
        pb.write_u8(o++, DHCP_OPT_LEASE);
        pb.write_u8(o++, 4);
        pb.write_u32_be(o, 3600);
        o += 4;

        // Option 1: Subnet Mask
        pb.write_u8(o++, DHCP_OPT_SUBNET);
        pb.write_u8(o++, 4);
        pb.write_bytes(o, EMU_SUBNET, 4);
        o += 4;

        // Option 3: Router
        pb.write_u8(o++, DHCP_OPT_ROUTER);
        pb.write_u8(o++, 4);
        pb.write_bytes(o, EMU_SERVER_IP, 4);
        o += 4;

        // Option 255: End
        pb.write_u8(o++, DHCP_OPT_END);

        recalc_ip_checksum(pkt);
        rx_queue_.push_back(std::move(pkt));
    }

    void recalc_icmp_checksum(std::vector<uint8_t>& frame, size_t icmp_offset) {
        // Clear checksum field
        frame[icmp_offset + 2] = 0;
        frame[icmp_offset + 3] = 0;

        // Calculate checksum over entire ICMP message
        size_t icmp_len = frame.size() - icmp_offset;
        uint32_t sum = 0;

        for (size_t i = 0; i < icmp_len; i += 2) {
            uint16_t word = frame[icmp_offset + i] << 8;
            if (i + 1 < icmp_len) {
                word |= frame[icmp_offset + i + 1];
            }
            sum += word;
        }

        // Fold 32-bit sum to 16 bits
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        uint16_t checksum = ~sum;
        frame[icmp_offset + 2] = (checksum >> 8) & 0xFF;
        frame[icmp_offset + 3] = checksum & 0xFF;
    }

    void recalc_ip_checksum(std::vector<uint8_t>& frame) {
        // IP header starts at offset 14
        size_t ip_start = 14;
        size_t ip_hdr_len = (frame[ip_start] & 0x0F) * 4;

        // Clear checksum field
        frame[ip_start + 10] = 0;
        frame[ip_start + 11] = 0;

        // Calculate checksum
        uint32_t sum = 0;
        for (size_t i = 0; i < ip_hdr_len; i += 2) {
            sum += (frame[ip_start + i] << 8) | frame[ip_start + i + 1];
        }

        // Fold 32-bit sum to 16 bits
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        uint16_t checksum = ~sum;
        frame[ip_start + 10] = (checksum >> 8) & 0xFF;
        frame[ip_start + 11] = checksum & 0xFF;
    }

    // TFTP: Handle initial RRQ/WRQ request to port 69
    void process_tftp_initial(const std::vector<uint8_t>& frame, uint16_t client_port) {
        if (tftp_root_.empty()) return;  // No TFTP root configured
        if (frame.size() < 44) return;   // Need at least opcode + some filename

        // Extract client info
        uint8_t client_mac[6];
        std::memcpy(client_mac, &frame[6], 6);
        uint8_t client_ip[4];
        std::memcpy(client_ip, &frame[26], 4);

        // TFTP data starts at offset 42
        uint16_t opcode = (frame[42] << 8) | frame[43];

        // Extract filename (null-terminated string after opcode)
        std::string filename;
        size_t i = 44;
        while (i < frame.size() && frame[i] != 0) {
            filename += static_cast<char>(frame[i++]);
        }

        if (filename.empty()) {
            send_tftp_error(client_mac, client_ip, client_port, TFTP_ERR_ILLEGAL_OP, "Empty filename");
            return;
        }

        // Skip mode string (we ignore it, always binary)
        // i++ to skip null, then skip mode string
        i++;
        while (i < frame.size() && frame[i] != 0) i++;

        if (opcode == TFTP_RRQ) {
            // Read Request
            handle_tftp_rrq(client_mac, client_ip, client_port, filename);
        } else if (opcode == TFTP_WRQ) {
            // Write Request
            handle_tftp_wrq(client_mac, client_ip, client_port, filename);
        } else {
            send_tftp_error(client_mac, client_ip, client_port, TFTP_ERR_ILLEGAL_OP, "Invalid opcode");
        }
    }

    // Handle TFTP Read Request
    void handle_tftp_rrq(const uint8_t* client_mac, const uint8_t* client_ip,
                         uint16_t client_port, const std::string& filename) {
        TftpSession session;
        session.client_port = client_port;
        std::memcpy(session.client_mac, client_mac, 6);
        std::memcpy(session.client_ip, client_ip, 4);
        session.is_read = true;
        session.block_num = 0;
        session.filename = filename;
        session.offset = 0;

        // Special case: /.dir returns directory listing
        if (filename == "/.dir" || filename == ".dir") {
            session.file_data = generate_dir_listing();
        } else {
            // Sanitize path - remove leading / and prevent ..
            std::string safe_name = filename;
            if (!safe_name.empty() && safe_name[0] == '/') {
                safe_name = safe_name.substr(1);
            }
            if (safe_name.find("..") != std::string::npos) {
                send_tftp_error(client_mac, client_ip, client_port, TFTP_ERR_ACCESS, "Invalid path");
                return;
            }

            // Build full path
            std::filesystem::path full_path = std::filesystem::path(tftp_root_) / safe_name;

            // Read file
            std::ifstream file(full_path, std::ios::binary);
            if (!file) {
                send_tftp_error(client_mac, client_ip, client_port, TFTP_ERR_NOT_FOUND, "File not found");
                return;
            }

            session.file_data = std::vector<uint8_t>(
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()
            );
        }

        // Store session and send first data block
        tftp_sessions_[client_port] = std::move(session);
        send_tftp_data_block(tftp_sessions_[client_port]);
    }

    // Handle TFTP Write Request
    void handle_tftp_wrq(const uint8_t* client_mac, const uint8_t* client_ip,
                         uint16_t client_port, const std::string& filename) {
        // Sanitize path
        std::string safe_name = filename;
        if (!safe_name.empty() && safe_name[0] == '/') {
            safe_name = safe_name.substr(1);
        }
        if (safe_name.find("..") != std::string::npos) {
            send_tftp_error(client_mac, client_ip, client_port, TFTP_ERR_ACCESS, "Invalid path");
            return;
        }

        TftpSession session;
        session.client_port = client_port;
        std::memcpy(session.client_mac, client_mac, 6);
        std::memcpy(session.client_ip, client_ip, 4);
        session.is_read = false;
        session.block_num = 0;
        session.filename = safe_name;
        session.offset = 0;

        // Store session and send ACK 0
        tftp_sessions_[client_port] = std::move(session);
        send_tftp_ack(tftp_sessions_[client_port], 0);
    }

    // Handle TFTP DATA or ACK during transfer
    void process_tftp_data(const std::vector<uint8_t>& frame, TftpSession& session) {
        if (frame.size() < 46) return;  // ETH + IP + UDP + opcode + block

        uint16_t opcode = (frame[42] << 8) | frame[43];
        uint16_t block = (frame[44] << 8) | frame[45];

        if (session.is_read && opcode == TFTP_ACK) {
            // Client ACKed our data block
            if (block == session.block_num) {
                // Check if transfer complete (last block was < 512 bytes)
                if (session.offset >= session.file_data.size()) {
                    // Transfer complete
                    tftp_sessions_.erase(session.client_port);
                    return;
                }
                // Send next block
                send_tftp_data_block(session);
            }
        } else if (!session.is_read && opcode == TFTP_DATA) {
            // Client sent data block
            if (block == session.block_num + 1) {
                session.block_num = block;

                // Append data (starts at offset 46)
                size_t data_len = frame.size() - 46;
                for (size_t i = 0; i < data_len; i++) {
                    session.file_data.push_back(frame[46 + i]);
                }

                // Send ACK
                send_tftp_ack(session, block);

                // If block < 512 bytes, transfer complete
                if (data_len < 512) {
                    // Write file to disk
                    std::filesystem::path full_path = std::filesystem::path(tftp_root_) / session.filename;

                    // Create parent directories if needed
                    std::filesystem::create_directories(full_path.parent_path());

                    std::ofstream file(full_path, std::ios::binary);
                    if (file) {
                        file.write(reinterpret_cast<const char*>(session.file_data.data()),
                                   session.file_data.size());
                    }

                    tftp_sessions_.erase(session.client_port);
                }
            }
        }
    }

    // Send TFTP data block
    void send_tftp_data_block(TftpSession& session) {
        session.block_num++;

        // Calculate data for this block (max 512 bytes)
        size_t remaining = session.file_data.size() - session.offset;
        size_t block_size = std::min(remaining, size_t(512));

        // Build packet: ETH(14) + IP(20) + UDP(8) + opcode(2) + block(2) + data
        size_t pkt_size = 14 + 20 + 8 + 4 + block_size;
        std::vector<uint8_t> pkt(pkt_size, 0);

        // Ethernet header
        std::memcpy(&pkt[0], session.client_mac, 6);
        std::memcpy(&pkt[6], EMU_SERVER_MAC, 6);
        pkt[12] = 0x08; pkt[13] = 0x00;

        // IP header
        pkt[14] = 0x45;
        uint16_t ip_len = 20 + 8 + 4 + block_size;
        pkt[16] = (ip_len >> 8) & 0xFF;
        pkt[17] = ip_len & 0xFF;
        pkt[22] = 64;  // TTL
        pkt[23] = IP_PROTO_UDP;
        std::memcpy(&pkt[26], EMU_SERVER_IP, 4);
        std::memcpy(&pkt[30], session.client_ip, 4);

        // UDP header
        uint16_t server_port = TFTP_PORT;  // Use port 69 for response
        pkt[34] = (server_port >> 8) & 0xFF;
        pkt[35] = server_port & 0xFF;
        pkt[36] = (session.client_port >> 8) & 0xFF;
        pkt[37] = session.client_port & 0xFF;
        uint16_t udp_len = 8 + 4 + block_size;
        pkt[38] = (udp_len >> 8) & 0xFF;
        pkt[39] = udp_len & 0xFF;

        // TFTP DATA
        pkt[42] = 0;
        pkt[43] = TFTP_DATA;
        pkt[44] = (session.block_num >> 8) & 0xFF;
        pkt[45] = session.block_num & 0xFF;

        // Copy data
        std::memcpy(&pkt[46], session.file_data.data() + session.offset, block_size);
        session.offset += block_size;

        recalc_ip_checksum(pkt);
        rx_queue_.push_back(std::move(pkt));
    }

    // Send TFTP ACK
    void send_tftp_ack(TftpSession& session, uint16_t block) {
        // Build packet: ETH(14) + IP(20) + UDP(8) + opcode(2) + block(2) = 46 bytes
        std::vector<uint8_t> pkt(46, 0);

        // Ethernet header
        std::memcpy(&pkt[0], session.client_mac, 6);
        std::memcpy(&pkt[6], EMU_SERVER_MAC, 6);
        pkt[12] = 0x08; pkt[13] = 0x00;

        // IP header
        pkt[14] = 0x45;
        uint16_t ip_len = 20 + 8 + 4;
        pkt[16] = (ip_len >> 8) & 0xFF;
        pkt[17] = ip_len & 0xFF;
        pkt[22] = 64;
        pkt[23] = IP_PROTO_UDP;
        std::memcpy(&pkt[26], EMU_SERVER_IP, 4);
        std::memcpy(&pkt[30], session.client_ip, 4);

        // UDP header
        uint16_t server_port = TFTP_PORT;
        pkt[34] = (server_port >> 8) & 0xFF;
        pkt[35] = server_port & 0xFF;
        pkt[36] = (session.client_port >> 8) & 0xFF;
        pkt[37] = session.client_port & 0xFF;
        pkt[38] = 0; pkt[39] = 12;  // UDP len = 8 + 4

        // TFTP ACK
        pkt[42] = 0;
        pkt[43] = TFTP_ACK;
        pkt[44] = (block >> 8) & 0xFF;
        pkt[45] = block & 0xFF;

        recalc_ip_checksum(pkt);
        rx_queue_.push_back(std::move(pkt));
    }

    // Send TFTP error
    void send_tftp_error(const uint8_t* client_mac, const uint8_t* client_ip,
                         uint16_t client_port, uint16_t error_code, const char* msg) {
        size_t msg_len = std::strlen(msg);
        // ETH(14) + IP(20) + UDP(8) + opcode(2) + errcode(2) + msg + null
        size_t pkt_size = 14 + 20 + 8 + 4 + msg_len + 1;
        std::vector<uint8_t> pkt(pkt_size, 0);

        // Ethernet header
        std::memcpy(&pkt[0], client_mac, 6);
        std::memcpy(&pkt[6], EMU_SERVER_MAC, 6);
        pkt[12] = 0x08; pkt[13] = 0x00;

        // IP header
        pkt[14] = 0x45;
        uint16_t ip_len = 20 + 8 + 4 + msg_len + 1;
        pkt[16] = (ip_len >> 8) & 0xFF;
        pkt[17] = ip_len & 0xFF;
        pkt[22] = 64;
        pkt[23] = IP_PROTO_UDP;
        std::memcpy(&pkt[26], EMU_SERVER_IP, 4);
        std::memcpy(&pkt[30], client_ip, 4);

        // UDP header
        pkt[34] = 0; pkt[35] = TFTP_PORT;
        pkt[36] = (client_port >> 8) & 0xFF;
        pkt[37] = client_port & 0xFF;
        uint16_t udp_len = 8 + 4 + msg_len + 1;
        pkt[38] = (udp_len >> 8) & 0xFF;
        pkt[39] = udp_len & 0xFF;

        // TFTP ERROR
        pkt[42] = 0;
        pkt[43] = TFTP_ERROR;
        pkt[44] = (error_code >> 8) & 0xFF;
        pkt[45] = error_code & 0xFF;
        std::memcpy(&pkt[46], msg, msg_len);
        // pkt[46 + msg_len] already 0 from vector init

        recalc_ip_checksum(pkt);
        rx_queue_.push_back(std::move(pkt));
    }

    // Generate directory listing for /.dir
    std::vector<uint8_t> generate_dir_listing() {
        std::string listing;
        namespace fs = std::filesystem;

        try {
            for (const auto& entry : fs::recursive_directory_iterator(tftp_root_)) {
                if (entry.is_regular_file()) {
                    // Get relative path
                    auto rel_path = fs::relative(entry.path(), tftp_root_);
                    std::string path_str = rel_path.generic_string();

                    // Get file size
                    auto size = entry.file_size();

                    // Format: path<TAB>size<LF>
                    listing += path_str + "\t" + std::to_string(size) + "\n";
                }
            }
        } catch (...) {
            // Ignore filesystem errors
        }

        return std::vector<uint8_t>(listing.begin(), listing.end());
    }
};

} // namespace cosmo
