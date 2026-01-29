# ICMP Echo (Ping) test
# Tests the emulator's built-in ICMP echo service

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

# ICMP frame size: ETH(14) + IP(20) + ICMP(8) + payload(4) = 46
.equ FRAME_SIZE,    46

_start:
    # Initialize stack
    lui     sp, 0x20010

    # Test 1: Setup MAC address
    li      t0, ETH_MACA0HR
    li      t1, 0x0002          # MAC: 00:02:xx:xx:xx:xx
    sw      t1, 0(t0)

    li      t0, ETH_MACA0LR
    li      t1, 0x03040506      # MAC: xx:xx:03:04:05:06
    sw      t1, 0(t0)

    # Test 2: Build ICMP Echo Request packet
    jal     build_icmp_request

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
    li      t1, 256 | RDES1_RCH
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

    # Test 10: Wait for RX (ICMP Echo Reply)
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
    # Test 11: Verify ICMP Echo Reply type (should be 0)
    # ICMP type is at offset 34 (ETH 14 + IP 20)
    li      t0, RX_BUF + 34
    lbu     t1, 0(t0)
    bnez    t1, fail11         # Type should be 0 (Echo Reply)

    # Test 12: Verify ICMP code (should be 0)
    li      t0, RX_BUF + 35
    lbu     t1, 0(t0)
    bnez    t1, fail12

    # Test 13: Verify ping data echoed back
    # Payload starts at offset 42
    li      t0, RX_BUF + 42
    lw      t1, 0(t0)
    li      t2, 0x50494E47      # "PING" in little-endian (actually "GNIP")
    bne     t1, t2, fail13

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

# Build ICMP Echo Request packet
# Sends ping to 10.0.0.1 with 4-byte payload "PING"
build_icmp_request:
    addi    sp, sp, -4
    sw      ra, 0(sp)

    li      t0, TX_BUF

    # Ethernet header (14 bytes)
    # Dest MAC: 02:00:00:00:00:01 (emulator server)
    li      t1, 0x00000002
    sw      t1, 0(t0)
    li      t1, 0x0100
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

    # Total length: 32 (20 IP + 8 ICMP + 4 payload)
    li      t1, 0x2000          # 0x0020 big-endian
    sh      t1, 16(t0)

    # ID, Flags, Fragment = 0
    sw      zero, 18(t0)

    # TTL: 64, Protocol: 1 (ICMP)
    li      t1, 0x0140          # TTL=64, Proto=1
    sh      t1, 22(t0)

    # Header checksum (will calculate)
    sh      zero, 24(t0)

    # Source IP: 10.0.0.2
    li      t1, 0x0200000A      # 10.0.0.2 little-endian
    sw      t1, 26(t0)

    # Dest IP: 10.0.0.1
    li      t1, 0x0100000A      # 10.0.0.1 little-endian
    sw      t1, 30(t0)

    # Calculate IP header checksum
    addi    a0, t0, 14
    jal     calc_ip_checksum

    # Reload t0 (clobbered by calc_ip_checksum)
    li      t0, TX_BUF

    # ICMP header (8 bytes) at offset 34
    # Type: 8 (Echo Request), Code: 0
    li      t1, 0x0008
    sh      t1, 34(t0)

    # Checksum (will calculate)
    sh      zero, 36(t0)

    # Identifier: 0x1234
    li      t1, 0x3412          # Big-endian as little-endian
    sh      t1, 38(t0)

    # Sequence: 0x0001
    li      t1, 0x0100          # Big-endian as little-endian
    sh      t1, 40(t0)

    # Payload: "PING" at offset 42
    li      t1, 0x50494E47      # "GNIP" -> "PING" when read as bytes
    sw      t1, 42(t0)

    # Calculate ICMP checksum (12 bytes: 8 header + 4 payload)
    addi    a0, t0, 34          # ICMP starts at offset 34
    li      a1, 12              # ICMP length
    jal     calc_icmp_checksum

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

# Calculate ICMP checksum
# a0 = pointer to ICMP data
# a1 = length in bytes
calc_icmp_checksum:
    li      t1, 0               # Sum
    mv      t2, a1              # Length
    mv      t6, a0              # Save start address

    # Clear checksum field (offset 2)
    sh      zero, 2(a0)

icmp_csum_loop:
    beqz    t2, icmp_csum_done
    lbu     t3, 0(a0)
    slli    t3, t3, 8
    addi    t2, t2, -1
    beqz    t2, icmp_csum_odd
    lbu     t4, 1(a0)
    or      t3, t3, t4
    addi    t2, t2, -1
    addi    a0, a0, 2
    add     t1, t1, t3
    j       icmp_csum_loop

icmp_csum_odd:
    add     t1, t1, t3

icmp_csum_done:
    # Fold
    srli    t2, t1, 16
    slli    t1, t1, 16
    srli    t1, t1, 16
    add     t1, t1, t2
    srli    t2, t1, 16
    add     t1, t1, t2
    slli    t1, t1, 16
    srli    t1, t1, 16

    # Complement
    not     t1, t1
    slli    t1, t1, 16
    srli    t1, t1, 16

    # Store checksum (big-endian)
    srli    t2, t1, 8
    sb      t2, 2(t6)
    sb      t1, 3(t6)
    ret
