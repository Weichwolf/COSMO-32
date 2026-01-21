# DHCP test
# Tests the emulator's built-in DHCP server
# Sends DHCP Discover, verifies DHCP Offer with correct IP

.section .text
.globl _start

# ETH MAC registers (at 0x40023000)
.equ ETH_BASE,      0x40023000
.equ ETH_MACCR,     0x40023000  # MAC Control
.equ ETH_MACA0HR,   0x40023008  # MAC Address High
.equ ETH_MACA0LR,   0x4002300C  # MAC Address Low
.equ ETH_DMAOMR,    0x40023010  # DMA Operation Mode
.equ ETH_DMASR,     0x40023014  # DMA Status
.equ ETH_DMATDLAR,  0x40023018  # TX Descriptor List
.equ ETH_DMARDLAR,  0x4002301C  # RX Descriptor List
.equ ETH_DMATPDR,   0x40023020  # TX Poll Demand

# MACCR bits
.equ MACCR_TE,      (1 << 0)    # TX Enable
.equ MACCR_RE,      (1 << 1)    # RX Enable

# DMAOMR bits
.equ DMAOMR_SR,     (1 << 0)    # Start Receive
.equ DMAOMR_ST,     (1 << 1)    # Start Transmit

# DMASR bits
.equ DMASR_TS,      (1 << 0)    # TX Status
.equ DMASR_RS,      (1 << 1)    # RX Status

# TDES0 bits
.equ TDES0_OWN,     (1 << 31)   # Owned by DMA
.equ TDES0_FS,      (1 << 28)   # First Segment
.equ TDES0_LS,      (1 << 29)   # Last Segment
.equ TDES0_TCH,     (1 << 20)   # Chained

# RDES0 bits
.equ RDES0_OWN,     (1 << 31)   # Owned by DMA

# RDES1 bits
.equ RDES1_RCH,     (1 << 14)   # Chained

# RAM addresses for descriptors and buffers
.equ TX_DESC,       0x20008000
.equ RX_DESC,       0x20008100
.equ TX_BUF,        0x20008200
.equ RX_BUF,        0x20008400

# DHCP Discover frame size: ETH(14) + IP(20) + UDP(8) + BOOTP(240) + Magic(4) + Options(9) = 295
# Padded to 300 bytes for safety
.equ FRAME_SIZE,    300

# DHCP message types
.equ DHCP_DISCOVER, 1
.equ DHCP_OFFER,    2
.equ DHCP_REQUEST,  3
.equ DHCP_ACK,      5

_start:
    # Initialize stack
    lui     sp, 0x20010

    # Test 1: Setup MAC address (00:02:03:04:05:06)
    li      t0, ETH_MACA0HR
    li      t1, 0x0002
    sw      t1, 0(t0)

    li      t0, ETH_MACA0LR
    li      t1, 0x03040506
    sw      t1, 0(t0)

    # Test 2: Build DHCP Discover packet
    jal     build_dhcp_discover

    # Test 3: Setup TX descriptor
    li      t0, TX_DESC
    li      t1, TDES0_OWN | TDES0_FS | TDES0_LS | TDES0_TCH
    sw      t1, 0(t0)
    li      t1, FRAME_SIZE
    sw      t1, 4(t0)
    li      t1, TX_BUF
    sw      t1, 8(t0)
    li      t1, TX_DESC
    sw      t1, 12(t0)

    # Test 4: Setup RX descriptor
    li      t0, RX_DESC
    li      t1, RDES0_OWN
    sw      t1, 0(t0)
    li      t1, 512 | RDES1_RCH
    sw      t1, 4(t0)
    li      t1, RX_BUF
    sw      t1, 8(t0)
    li      t1, RX_DESC
    sw      t1, 12(t0)

    # Test 5: Set descriptor list addresses
    li      t0, ETH_DMATDLAR
    li      t1, TX_DESC
    sw      t1, 0(t0)

    li      t0, ETH_DMARDLAR
    li      t1, RX_DESC
    sw      t1, 0(t0)

    # Test 6: Enable MAC TX/RX
    li      t0, ETH_MACCR
    li      t1, MACCR_TE | MACCR_RE
    sw      t1, 0(t0)

    # Test 7: Start DMA TX/RX
    li      t0, ETH_DMAOMR
    li      t1, DMAOMR_ST | DMAOMR_SR
    sw      t1, 0(t0)

    # Test 8: Trigger TX poll
    li      t0, ETH_DMATPDR
    sw      zero, 0(t0)

    # Test 9: Wait for TX complete
    li      t0, ETH_DMASR
    li      t3, 100000
wait_tx:
    lw      t1, 0(t0)
    li      t2, DMASR_TS
    and     t4, t1, t2
    bnez    t4, tx_done
    addi    t3, t3, -1
    bnez    t3, wait_tx
    j       fail9

tx_done:
    # Clear TX status
    li      t0, ETH_DMASR
    li      t1, DMASR_TS
    sw      t1, 0(t0)

    # Test 10: Wait for RX (DHCP Offer)
    li      t0, ETH_DMASR
    li      t3, 100000
wait_rx:
    lw      t1, 0(t0)
    li      t2, DMASR_RS
    and     t4, t1, t2
    bnez    t4, rx_done
    addi    t3, t3, -1
    bnez    t3, wait_rx
    j       fail10

rx_done:
    # Test 11: Verify BOOTP op field (should be 2 = reply)
    # BOOTP op is at offset 42 (ETH 14 + IP 20 + UDP 8)
    li      t0, RX_BUF + 42
    lbu     t1, 0(t0)
    li      t2, 2               # BOOTREPLY
    bne     t1, t2, fail11

    # Test 12: Verify yiaddr (Your IP) = 10.0.0.2
    # yiaddr is at BOOTP offset 16, so offset 42 + 16 = 58
    li      t0, RX_BUF + 58
    lw      t1, 0(t0)
    li      t2, 0x0200000A      # 10.0.0.2 little-endian
    bne     t1, t2, fail12

    # Test 13: Verify DHCP message type in options = OFFER (2)
    # Magic cookie at offset 42 + 236 = 278
    # First option at 282, message type option: 53, len, type
    li      t0, RX_BUF + 282    # Start of options after magic cookie
    lbu     t1, 0(t0)           # Option type should be 53 (DHCP Message Type)
    li      t2, 53
    bne     t1, t2, fail13

    # Test 14: Verify option value is DHCP_OFFER (2)
    li      t0, RX_BUF + 284    # Option value (after type + length)
    lbu     t1, 0(t0)
    li      t2, DHCP_OFFER
    bne     t1, t2, fail14

    # All tests passed
pass:
    li      gp, 1
    li      a0, 0
    ecall

fail9:
    li      gp, 19
    li      a0, 1
    ecall

fail10:
    li      gp, 21
    li      a0, 1
    ecall

fail11:
    li      gp, 23
    li      a0, 1
    ecall

fail12:
    li      gp, 25
    li      a0, 1
    ecall

fail13:
    li      gp, 27
    li      a0, 1
    ecall

fail14:
    li      gp, 29
    li      a0, 1
    ecall

# Build DHCP Discover packet
build_dhcp_discover:
    addi    sp, sp, -4
    sw      ra, 0(sp)

    li      t0, TX_BUF

    # Ethernet header (14 bytes)
    # Dest MAC: FF:FF:FF:FF:FF:FF (broadcast)
    li      t1, 0xFFFFFFFF
    sw      t1, 0(t0)
    li      t1, 0xFFFF
    sh      t1, 4(t0)

    # Source MAC: 00:02:03:04:05:06
    li      t1, 0x0302
    sh      t1, 6(t0)
    li      t1, 0x06050403
    sw      t1, 8(t0)

    # EtherType: 0x0800 (IPv4)
    li      t1, 0x0008          # Big-endian as little-endian
    sh      t1, 12(t0)

    # IP header (20 bytes) at offset 14
    # Version/IHL: 0x45
    li      t1, 0x0045
    sh      t1, 14(t0)

    # Total length: 20 (IP) + 8 (UDP) + 244 (BOOTP min) = 272 = 0x0110
    li      t1, 0x1001          # 0x0110 big-endian
    sh      t1, 16(t0)

    # ID, Flags, Fragment = 0
    sw      zero, 18(t0)

    # TTL: 64, Protocol: 17 (UDP)
    li      t1, 0x1140          # TTL=64, Proto=17
    sh      t1, 22(t0)

    # Header checksum (will calculate)
    sh      zero, 24(t0)

    # Source IP: 0.0.0.0
    sw      zero, 26(t0)

    # Dest IP: 255.255.255.255 (broadcast)
    li      t1, 0xFFFFFFFF
    sw      t1, 30(t0)

    # Calculate IP header checksum
    addi    a0, t0, 14
    jal     calc_ip_checksum

    # Reload t0
    li      t0, TX_BUF

    # UDP header (8 bytes) at offset 34
    # Source port: 68 (DHCP client)
    li      t1, 0x4400          # 68 big-endian
    sh      t1, 34(t0)

    # Dest port: 67 (DHCP server)
    li      t1, 0x4300          # 67 big-endian
    sh      t1, 36(t0)

    # UDP length: 8 + 244 = 252 = 0x00FC
    li      t1, 0xFC00          # 0x00FC big-endian
    sh      t1, 38(t0)

    # UDP checksum: 0 (optional)
    sh      zero, 40(t0)

    # BOOTP/DHCP (236 bytes) at offset 42
    # op: 1 (BOOTREQUEST)
    li      t1, 1
    sb      t1, 42(t0)

    # htype: 1 (Ethernet)
    sb      t1, 43(t0)

    # hlen: 6
    li      t1, 6
    sb      t1, 44(t0)

    # hops: 0
    sb      zero, 45(t0)

    # xid: 0x12345678 (transaction ID)
    li      t1, 0x12345678
    sw      t1, 46(t0)

    # secs: 0
    sh      zero, 50(t0)

    # flags: 0x8000 (broadcast)
    li      t1, 0x0080          # 0x8000 big-endian
    sh      t1, 52(t0)

    # ciaddr: 0 (client IP)
    sw      zero, 54(t0)

    # yiaddr: 0 (your IP)
    sw      zero, 58(t0)

    # siaddr: 0 (server IP)
    sw      zero, 62(t0)

    # giaddr: 0 (gateway IP)
    sw      zero, 66(t0)

    # chaddr: client MAC (16 bytes, only first 6 used)
    # Store at offset 42 + 28 = 70
    li      t1, 0x03020002      # 00:02:03:xx little-endian
    sw      t1, 70(t0)
    li      t1, 0x00000605      # 04:05:06:00 little-endian shifted
    sw      t1, 74(t0)
    sw      zero, 78(t0)
    sw      zero, 82(t0)

    # sname: 0 (64 bytes at offset 44 in BOOTP = 86 absolute)
    # file: 0 (128 bytes at offset 108 in BOOTP = 150 absolute)
    # Just leave as zeros (already cleared)

    # DHCP Magic cookie at offset 236 in BOOTP = 278 absolute
    # 0x63825363
    li      t1, 0x63
    sb      t1, 278(t0)
    li      t1, 0x82
    sb      t1, 279(t0)
    li      t1, 0x53
    sb      t1, 280(t0)
    li      t1, 0x63
    sb      t1, 281(t0)

    # DHCP Options at offset 282
    # Option 53 (DHCP Message Type): len=1, value=1 (DISCOVER)
    li      t1, 53
    sb      t1, 282(t0)
    li      t1, 1
    sb      t1, 283(t0)
    li      t1, DHCP_DISCOVER
    sb      t1, 284(t0)

    # Option 255 (End)
    li      t1, 255
    sb      t1, 285(t0)

    lw      ra, 0(sp)
    addi    sp, sp, 4
    ret

# Calculate IP header checksum
# a0 = pointer to IP header
calc_ip_checksum:
    li      t1, 0               # Sum
    li      t2, 10              # 10 halfwords

    sh      zero, 10(a0)        # Clear checksum

checksum_loop:
    lhu     t3, 0(a0)
    # Swap bytes
    srli    t4, t3, 8
    slli    t5, t3, 8
    slli    t5, t5, 16
    srli    t5, t5, 16
    or      t3, t4, t5
    add     t1, t1, t3
    addi    a0, a0, 2
    addi    t2, t2, -1
    bnez    t2, checksum_loop

    # Fold and complement
    srli    t2, t1, 16
    slli    t1, t1, 16
    srli    t1, t1, 16
    add     t1, t1, t2
    srli    t2, t1, 16
    add     t1, t1, t2
    slli    t1, t1, 16
    srli    t1, t1, 16
    not     t1, t1
    slli    t1, t1, 16
    srli    t1, t1, 16

    # Swap bytes for storage
    srli    t2, t1, 8
    slli    t3, t1, 8
    slli    t3, t3, 16
    srli    t3, t3, 16
    or      t1, t2, t3

    li      t0, TX_BUF + 24
    sh      t1, 0(t0)
    ret
