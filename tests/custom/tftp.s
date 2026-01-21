# TFTP RRQ Test - Read Request for /.dir
# Verifies TFTP server responds with directory listing

.section .text
.globl _start

# ETH MAC registers (at 0x40023000)
.equ ETH_BASE,      0x40023000
.equ ETH_MACCR,     0x40023000
.equ ETH_MACA0HR,   0x40023008
.equ ETH_MACA0LR,   0x4002300C
.equ ETH_DMAOMR,    0x40023010
.equ ETH_DMASR,     0x40023014
.equ ETH_DMATDLAR,  0x40023018
.equ ETH_DMARDLAR,  0x4002301C
.equ ETH_DMATPDR,   0x40023020

# MACCR bits
.equ MACCR_TE,      (1 << 0)
.equ MACCR_RE,      (1 << 1)

# DMAOMR bits
.equ DMAOMR_SR,     (1 << 0)
.equ DMAOMR_ST,     (1 << 1)

# DMASR bits
.equ DMASR_TS,      (1 << 0)
.equ DMASR_RS,      (1 << 1)

# TDES0 bits
.equ TDES0_OWN,     (1 << 31)
.equ TDES0_FS,      (1 << 28)
.equ TDES0_LS,      (1 << 29)
.equ TDES0_TCH,     (1 << 20)

# RDES0 bits
.equ RDES0_OWN,     (1 << 31)

# RDES1 bits
.equ RDES1_RCH,     (1 << 14)

# RAM addresses for descriptors and buffers
.equ TX_DESC,       0x20008000
.equ RX_DESC,       0x20008100
.equ TX_BUF,        0x20008200
.equ RX_BUF,        0x20008600

# TFTP Frame: ETH(14) + IP(20) + UDP(8) + opcode(2) + "/.dir\0"(6) + "octet\0"(6) = 56 bytes
.equ TFTP_RRQ_SIZE, 56

_start:
    lui     sp, 0x20010

    # Set MAC address
    li      t0, ETH_MACA0HR
    li      t1, 0x0002
    sw      t1, 0(t0)

    li      t0, ETH_MACA0LR
    li      t1, 0x03040506
    sw      t1, 0(t0)

    # Build TFTP RRQ frame
    jal     build_tftp_rrq

    # Setup TX descriptor
    li      t0, TX_DESC
    li      t1, TDES0_OWN | TDES0_FS | TDES0_LS | TDES0_TCH
    sw      t1, 0(t0)
    li      t1, TFTP_RRQ_SIZE
    sw      t1, 4(t0)
    li      t1, TX_BUF
    sw      t1, 8(t0)
    li      t1, TX_DESC
    sw      t1, 12(t0)

    # Setup RX descriptor
    li      t0, RX_DESC
    li      t1, RDES0_OWN
    sw      t1, 0(t0)
    li      t1, 1024 | RDES1_RCH
    sw      t1, 4(t0)
    li      t1, RX_BUF
    sw      t1, 8(t0)
    li      t1, RX_DESC
    sw      t1, 12(t0)

    # Set descriptor addresses
    li      t0, ETH_DMATDLAR
    li      t1, TX_DESC
    sw      t1, 0(t0)

    li      t0, ETH_DMARDLAR
    li      t1, RX_DESC
    sw      t1, 0(t0)

    # Enable MAC TX/RX
    li      t0, ETH_MACCR
    li      t1, MACCR_TE | MACCR_RE
    sw      t1, 0(t0)

    # Start DMA TX/RX
    li      t0, ETH_DMAOMR
    li      t1, DMAOMR_ST | DMAOMR_SR
    sw      t1, 0(t0)

    # Trigger TX poll
    li      t0, ETH_DMATPDR
    sw      zero, 0(t0)

    # Wait for TX complete
    li      t0, ETH_DMASR
    li      t3, 100000
wait_tx:
    lw      t1, 0(t0)
    li      t2, DMASR_TS
    and     t4, t1, t2
    bnez    t4, tx_done
    addi    t3, t3, -1
    bnez    t3, wait_tx
    j       fail_tx

tx_done:
    # Clear TX status
    li      t0, ETH_DMASR
    li      t1, DMASR_TS
    sw      t1, 0(t0)

    # Wait for RX (TFTP DATA response)
    li      t0, ETH_DMASR
    li      t3, 100000
wait_rx:
    lw      t1, 0(t0)
    li      t2, DMASR_RS
    and     t4, t1, t2
    bnez    t4, rx_done
    addi    t3, t3, -1
    bnez    t3, wait_rx
    j       fail_rx

rx_done:
    # Verify RX descriptor OWN cleared
    li      t0, RX_DESC
    lw      t1, 0(t0)
    lui     t2, 0x80000
    and     t3, t1, t2
    bnez    t3, fail_own

    # Check TFTP opcode at offset 42 = DATA (0x0003)
    li      t0, RX_BUF + 42
    lhu     t1, 0(t0)
    li      t2, 0x0300          # opcode 3 in big-endian (stored little-endian)
    bne     t1, t2, fail_opcode

    # Check block number at offset 44 = 1 (0x0001)
    li      t0, RX_BUF + 44
    lhu     t1, 0(t0)
    li      t2, 0x0100          # block 1 in big-endian
    bne     t1, t2, fail_block

    # Success - received TFTP DATA block 1
pass:
    li      gp, 1
    li      a0, 0
    ecall

fail_tx:
    li      gp, 3
    li      a0, 1
    ecall

fail_rx:
    li      gp, 5
    li      a0, 1
    ecall

fail_own:
    li      gp, 7
    li      a0, 1
    ecall

fail_opcode:
    li      gp, 9
    li      a0, 1
    ecall

fail_block:
    li      gp, 11
    li      a0, 1
    ecall

# Build TFTP RRQ packet for "/.dir" file
build_tftp_rrq:
    addi    sp, sp, -4
    sw      ra, 0(sp)

    li      t0, TX_BUF

    # Ethernet header (14 bytes)
    # Dest MAC: 02:00:00:00:00:01 (server MAC)
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
    li      t1, 0x0008
    sh      t1, 12(t0)

    # IP header (20 bytes) at offset 14
    # Version/IHL: 0x45
    li      t1, 0x0045
    sh      t1, 14(t0)

    # Total length: 42 (20 IP + 8 UDP + 14 TFTP)
    # TFTP: opcode(2) + "/.dir\0"(6) + "octet\0"(6) = 14
    li      t1, 0x2A00          # 42 in big-endian
    sh      t1, 16(t0)

    # ID, Flags, Fragment = 0
    sw      zero, 18(t0)

    # TTL: 64, Protocol: 17 (UDP)
    li      t1, 0x1140
    sh      t1, 22(t0)

    # Checksum (will calculate)
    sh      zero, 24(t0)

    # Source IP: 10.0.0.2
    li      t1, 0x0200000A
    sw      t1, 26(t0)

    # Dest IP: 10.0.0.1 (server)
    li      t1, 0x0100000A
    sw      t1, 30(t0)

    # Calculate IP checksum
    addi    a0, t0, 14
    jal     calc_ip_checksum

    li      t0, TX_BUF

    # UDP header (8 bytes) at offset 34
    # Source port: 1234 (0x04D2)
    li      t1, 0xD204
    sh      t1, 34(t0)

    # Dest port: 69 (TFTP)
    li      t1, 0x4500
    sh      t1, 36(t0)

    # UDP length: 22 (8 + 14)
    li      t1, 0x1600
    sh      t1, 38(t0)

    # UDP checksum: 0 (optional)
    sh      zero, 40(t0)

    # TFTP RRQ at offset 42
    # Opcode: 1 (RRQ) - big endian 0x0001
    li      t1, 0x0100
    sh      t1, 42(t0)

    # Filename: "/.dir" + null (7 bytes at offset 44)
    # '/' = 0x2F, '.' = 0x2E, 'd' = 0x64, 'i' = 0x69, 'r' = 0x72
    li      t1, 0x2E2F          # "/." little-endian
    sh      t1, 44(t0)
    li      t1, 0x6964          # "di" little-endian
    sh      t1, 46(t0)
    li      t1, 0x0072          # "r\0"
    sh      t1, 48(t0)

    # Mode: "octet" + null (6 bytes at offset 50)
    # 'o' = 0x6F, 'c' = 0x63, 't' = 0x74, 'e' = 0x65
    li      t1, 0x636F          # "oc"
    sh      t1, 50(t0)
    li      t1, 0x6574          # "te"
    sh      t1, 52(t0)
    li      t1, 0x0074          # "t\0"
    sh      t1, 54(t0)

    lw      ra, 0(sp)
    addi    sp, sp, 4
    ret

# IP checksum calculation (same as eth.s)
calc_ip_checksum:
    li      t1, 0
    li      t2, 10
    sh      zero, 10(a0)

checksum_loop:
    lhu     t3, 0(a0)
    srli    t4, t3, 8
    slli    t5, t3, 8
    slli    t5, t5, 16
    srli    t5, t5, 16
    or      t3, t4, t5
    add     t1, t1, t3
    addi    a0, a0, 2
    addi    t2, t2, -1
    bnez    t2, checksum_loop

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

    srli    t2, t1, 8
    slli    t3, t1, 8
    slli    t3, t3, 16
    srli    t3, t3, 16
    or      t1, t2, t3

    li      t0, TX_BUF + 24
    sh      t1, 0(t0)
    ret
