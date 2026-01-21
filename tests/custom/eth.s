# Ethernet MAC + UDP Echo test

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
.equ TDES0_IC,      (1 << 30)   # Interrupt on Completion
.equ TDES0_LS,      (1 << 29)   # Last Segment
.equ TDES0_FS,      (1 << 28)   # First Segment
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

# Frame size (ETH=14 + IP=20 + UDP=8 + payload=4 = 46)
.equ FRAME_SIZE,    46

_start:
    # Initialize stack
    lui     sp, 0x20010

    # Test 1: Set MAC address
    li      t0, ETH_MACA0HR
    li      t1, 0x0002          # MAC high: 00:02
    sw      t1, 0(t0)

    li      t0, ETH_MACA0LR
    li      t1, 0x03040506      # MAC low: 03:04:05:06
    sw      t1, 0(t0)

    # Verify MAC address
    li      t0, ETH_MACA0HR
    lw      t2, 0(t0)
    li      t1, 0x0002
    bne     t2, t1, fail1

    # Test 2: Build TX frame (UDP to port 7)
    jal     build_udp_frame

    # Test 3: Setup TX descriptor
    li      t0, TX_DESC

    # TDES0: OWN | FS | LS | TCH
    li      t1, TDES0_OWN | TDES0_FS | TDES0_LS | TDES0_TCH
    sw      t1, 0(t0)

    # TDES1: Buffer size
    li      t1, FRAME_SIZE
    sw      t1, 4(t0)

    # TDES2: Buffer address
    li      t1, TX_BUF
    sw      t1, 8(t0)

    # TDES3: Next descriptor (chain to self)
    li      t1, TX_DESC
    sw      t1, 12(t0)

    # Test 4: Setup RX descriptor
    li      t0, RX_DESC

    # RDES0: OWN
    li      t1, RDES0_OWN
    sw      t1, 0(t0)

    # RDES1: Buffer size | RCH
    li      t1, 256 | RDES1_RCH
    sw      t1, 4(t0)

    # RDES2: Buffer address
    li      t1, RX_BUF
    sw      t1, 8(t0)

    # RDES3: Next descriptor (chain to self)
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

    # Test 9: Wait for TX complete (longer timeout, check more often)
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
    # Test 10: Verify TX descriptor OWN bit cleared
    li      t0, TX_DESC
    lw      t1, 0(t0)
    lui     t2, 0x80000         # TDES0_OWN = bit 31
    and     t3, t1, t2
    bnez    t3, fail10          # OWN should be cleared

    # Clear TX status
    li      t0, ETH_DMASR
    li      t1, DMASR_TS
    sw      t1, 0(t0)

    # Test 11: Wait for RX (echo response) - longer timeout
    li      t0, ETH_DMASR
    li      t3, 100000
wait_rx:
    lw      t1, 0(t0)
    li      t2, DMASR_RS
    and     t4, t1, t2
    bnez    t4, rx_done
    addi    t3, t3, -1
    bnez    t3, wait_rx
    j       fail11

rx_done:
    # Test 12: Verify RX descriptor released (OWN cleared)
    li      t0, RX_DESC
    lw      t1, 0(t0)
    lui     t2, 0x80000         # RDES0_OWN = bit 31
    and     t3, t1, t2
    bnez    t3, fail12          # OWN should be cleared

    # Test 13: Verify echoed payload matches
    # Original payload was "TEST" (0x54455354)
    # Check UDP payload in RX buffer (offset 42)
    li      t0, RX_BUF + 42
    lw      t1, 0(t0)
    li      t2, 0x54534554      # "TEST" in little-endian
    bne     t1, t2, fail13

    # All tests passed
pass:
    li      gp, 1
    li      a0, 0
    ecall

fail1:
    li      gp, 3
    li      a0, 1
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

# Build a minimal UDP frame for echo test
# Sends to port 7 (echo) with 4 bytes payload "TEST"
build_udp_frame:
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
    li      t1, 0x0008          # Big-endian stored as little-endian bytes
    sh      t1, 12(t0)

    # IP header (20 bytes) at offset 14
    # Version/IHL: 0x45, DSCP: 0x00
    li      t1, 0x0045
    sh      t1, 14(t0)

    # Total length: 32 (20 IP + 8 UDP + 4 payload)
    li      t1, 0x2000          # 0x0020 in big-endian
    sh      t1, 16(t0)

    # ID, Flags, Fragment
    sw      zero, 18(t0)

    # TTL: 64, Protocol: 17 (UDP)
    li      t1, 0x1140          # TTL=64, Proto=17
    sh      t1, 22(t0)

    # Checksum (will calculate after)
    sh      zero, 24(t0)

    # Source IP: 10.0.0.2
    li      t1, 0x0200000A      # 10.0.0.2 in little-endian
    sw      t1, 26(t0)

    # Dest IP: 10.0.0.1
    li      t1, 0x0100000A      # 10.0.0.1 in little-endian
    sw      t1, 30(t0)

    # Calculate IP header checksum
    addi    a0, t0, 14          # IP header start
    jal     calc_ip_checksum

    # Reload t0 (clobbered by calc_ip_checksum)
    li      t0, TX_BUF

    # UDP header (8 bytes) at offset 34
    # Source port: 12345 (0x3039)
    li      t1, 0x3930          # Big-endian as little-endian bytes
    sh      t1, 34(t0)

    # Dest port: 7 (echo)
    li      t1, 0x0700
    sh      t1, 36(t0)

    # Length: 12 (8 header + 4 payload)
    li      t1, 0x0C00
    sh      t1, 38(t0)

    # UDP checksum: 0 (optional in IPv4)
    sh      zero, 40(t0)

    # Payload: "TEST" at offset 42
    li      t1, 0x54534554      # "TEST" in little-endian
    sw      t1, 42(t0)

    lw      ra, 0(sp)
    addi    sp, sp, 4
    ret

# Calculate IP header checksum
# a0 = pointer to IP header (20 bytes)
calc_ip_checksum:
    li      t1, 0               # Sum
    li      t2, 10              # 10 halfwords

    # Clear checksum field first
    sh      zero, 10(a0)

checksum_loop:
    lhu     t3, 0(a0)
    # Swap bytes for big-endian
    srli    t4, t3, 8
    slli    t5, t3, 8
    # Mask to 0xFF00: shift left 16, then right 16 clears low byte
    slli    t5, t5, 16
    srli    t5, t5, 16
    or      t3, t4, t5
    add     t1, t1, t3
    addi    a0, a0, 2
    addi    t2, t2, -1
    bnez    t2, checksum_loop

    # Fold 32-bit to 16-bit
    srli    t2, t1, 16
    # Mask t1 to 16 bits
    slli    t1, t1, 16
    srli    t1, t1, 16
    add     t1, t1, t2
    srli    t2, t1, 16
    add     t1, t1, t2
    # Mask to 16 bits
    slli    t1, t1, 16
    srli    t1, t1, 16

    # One's complement: XOR with 0xFFFF = NOT lower 16 bits
    not     t1, t1
    slli    t1, t1, 16
    srli    t1, t1, 16

    # Swap back for storage
    srli    t2, t1, 8
    slli    t3, t1, 8
    # Mask t3 to 0xFF00
    slli    t3, t3, 16
    srli    t3, t3, 16
    or      t1, t2, t3

    # Store checksum
    li      t0, TX_BUF + 24
    sh      t1, 0(t0)

    ret
