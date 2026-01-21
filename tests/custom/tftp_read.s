# TFTP Read Test - Read data/readme.txt and verify content
.section .text
.globl _start

.equ ETH_BASE,      0x40023000
.equ ETH_MACCR,     0x40023000
.equ ETH_MACA0HR,   0x40023008
.equ ETH_MACA0LR,   0x4002300C
.equ ETH_DMAOMR,    0x40023010
.equ ETH_DMASR,     0x40023014
.equ ETH_DMATDLAR,  0x40023018
.equ ETH_DMARDLAR,  0x4002301C
.equ ETH_DMATPDR,   0x40023020

.equ MACCR_TE,      (1 << 0)
.equ MACCR_RE,      (1 << 1)
.equ DMAOMR_SR,     (1 << 0)
.equ DMAOMR_ST,     (1 << 1)
.equ DMASR_TS,      (1 << 0)
.equ DMASR_RS,      (1 << 1)

.equ TDES0_OWN,     (1 << 31)
.equ TDES0_FS,      (1 << 28)
.equ TDES0_LS,      (1 << 29)
.equ TDES0_TCH,     (1 << 20)
.equ RDES0_OWN,     (1 << 31)
.equ RDES1_RCH,     (1 << 14)

.equ TX_DESC,       0x20008000
.equ RX_DESC,       0x20008100
.equ TX_BUF,        0x20008200
.equ RX_BUF,        0x20008800

_start:
    lui     sp, 0x20010

    # Setup MAC address
    li      t0, ETH_MACA0HR
    li      t1, 0x0002
    sw      t1, 0(t0)
    li      t0, ETH_MACA0LR
    li      t1, 0x03040506
    sw      t1, 0(t0)

    # Enable MAC + DMA
    li      t0, ETH_MACCR
    li      t1, MACCR_TE | MACCR_RE
    sw      t1, 0(t0)
    li      t0, ETH_DMAOMR
    li      t1, DMAOMR_ST | DMAOMR_SR
    sw      t1, 0(t0)

    # Build and send RRQ for "data/readme.txt"
    li      t0, TX_BUF

    # Ethernet header (14 bytes)
    # Dest MAC: 02:00:00:00:00:01
    li      t1, 0x02
    sb      t1, 0(t0)
    sb      zero, 1(t0)
    sb      zero, 2(t0)
    sb      zero, 3(t0)
    sb      zero, 4(t0)
    li      t1, 0x01
    sb      t1, 5(t0)

    # Src MAC: 00:02:03:04:05:06
    sb      zero, 6(t0)
    li      t1, 0x02
    sb      t1, 7(t0)
    li      t1, 0x03
    sb      t1, 8(t0)
    li      t1, 0x04
    sb      t1, 9(t0)
    li      t1, 0x05
    sb      t1, 10(t0)
    li      t1, 0x06
    sb      t1, 11(t0)

    # EtherType: 0x0800
    li      t1, 0x08
    sb      t1, 12(t0)
    sb      zero, 13(t0)

    # IP header (20 bytes)
    li      t1, 0x45
    sb      t1, 14(t0)
    sb      zero, 15(t0)        # DSCP

    # Total length: 20 (IP) + 8 (UDP) + 2 (opcode) + 16 (filename+null) + 6 (mode+null) = 52
    sb      zero, 16(t0)
    li      t1, 52
    sb      t1, 17(t0)

    # ID
    sh      zero, 18(t0)
    # Flags + Fragment
    sh      zero, 20(t0)

    # TTL=64, Proto=17 (UDP)
    li      t1, 64
    sb      t1, 22(t0)
    li      t1, 17
    sb      t1, 23(t0)

    # Checksum - skip (will use 0)
    sh      zero, 24(t0)

    # Src IP: 10.0.0.2
    li      t1, 10
    sb      t1, 26(t0)
    sb      zero, 27(t0)
    sb      zero, 28(t0)
    li      t1, 2
    sb      t1, 29(t0)

    # Dst IP: 10.0.0.1
    li      t1, 10
    sb      t1, 30(t0)
    sb      zero, 31(t0)
    sb      zero, 32(t0)
    li      t1, 1
    sb      t1, 33(t0)

    # UDP header (8 bytes)
    # Src port: 1234 = 0x04D2 (big-endian: 0x04, 0xD2)
    li      t1, 0x04
    sb      t1, 34(t0)
    li      t1, 0xD2
    sb      t1, 35(t0)

    # Dst port: 69 = 0x0045
    sb      zero, 36(t0)
    li      t1, 69
    sb      t1, 37(t0)

    # UDP length: 8 + 2 + 16 + 6 = 32
    sb      zero, 38(t0)
    li      t1, 32
    sb      t1, 39(t0)

    # UDP checksum: 0
    sh      zero, 40(t0)

    # TFTP RRQ (opcode = 1)
    sb      zero, 42(t0)
    li      t1, 1
    sb      t1, 43(t0)

    # Filename: "data/readme.txt" (15 chars + null = 16)
    # d=0x64, a=0x61, t=0x74, /=0x2F, r=0x72, e=0x65, m=0x6D, .=0x2E, x=0x78
    li      t1, 0x64    # d
    sb      t1, 44(t0)
    li      t1, 0x61    # a
    sb      t1, 45(t0)
    li      t1, 0x74    # t
    sb      t1, 46(t0)
    li      t1, 0x61    # a
    sb      t1, 47(t0)
    li      t1, 0x2F    # /
    sb      t1, 48(t0)
    li      t1, 0x72    # r
    sb      t1, 49(t0)
    li      t1, 0x65    # e
    sb      t1, 50(t0)
    li      t1, 0x61    # a
    sb      t1, 51(t0)
    li      t1, 0x64    # d
    sb      t1, 52(t0)
    li      t1, 0x6D    # m
    sb      t1, 53(t0)
    li      t1, 0x65    # e
    sb      t1, 54(t0)
    li      t1, 0x2E    # .
    sb      t1, 55(t0)
    li      t1, 0x74    # t
    sb      t1, 56(t0)
    li      t1, 0x78    # x
    sb      t1, 57(t0)
    li      t1, 0x74    # t
    sb      t1, 58(t0)
    sb      zero, 59(t0)  # null terminator

    # Mode: "octet" (5 chars + null = 6)
    li      t1, 0x6F    # o
    sb      t1, 60(t0)
    li      t1, 0x63    # c
    sb      t1, 61(t0)
    li      t1, 0x74    # t
    sb      t1, 62(t0)
    li      t1, 0x65    # e
    sb      t1, 63(t0)
    li      t1, 0x74    # t
    sb      t1, 64(t0)
    sb      zero, 65(t0)  # null

    # Total packet: 14 + 52 = 66 bytes

    # Setup TX descriptor
    li      t0, TX_DESC
    li      t1, TDES0_OWN | TDES0_FS | TDES0_LS | TDES0_TCH
    sw      t1, 0(t0)
    li      t1, 66
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

    # Trigger TX
    li      t0, ETH_DMATPDR
    sw      zero, 0(t0)

    # Wait for TX complete
    li      t0, ETH_DMASR
    li      t3, 100000
wait_tx:
    lw      t1, 0(t0)
    andi    t2, t1, DMASR_TS
    bnez    t2, tx_done
    addi    t3, t3, -1
    bnez    t3, wait_tx
    j       fail_tx

tx_done:
    # Clear TX status
    li      t1, DMASR_TS
    sw      t1, 0(t0)

    # Wait for RX
    li      t0, ETH_DMASR
    li      t3, 100000
wait_rx:
    lw      t1, 0(t0)
    andi    t2, t1, DMASR_RS
    bnez    t2, rx_done
    addi    t3, t3, -1
    bnez    t3, wait_rx
    j       fail_rx

rx_done:
    # Verify response opcode = DATA (0x0003)
    li      t0, RX_BUF + 42
    lbu     t1, 0(t0)   # high byte
    lbu     t2, 1(t0)   # low byte
    bnez    t1, fail_opcode
    li      t3, 3
    bne     t2, t3, fail_opcode

    # Verify block = 1
    lbu     t1, 2(t0)
    lbu     t2, 3(t0)
    bnez    t1, fail_block
    li      t3, 1
    bne     t2, t3, fail_block

    # Verify content starts with "COSMO"
    # Data at offset 46
    li      t0, RX_BUF + 46
    lbu     t1, 0(t0)
    li      t2, 0x43    # 'C'
    bne     t1, t2, fail_content
    lbu     t1, 1(t0)
    li      t2, 0x4F    # 'O'
    bne     t1, t2, fail_content
    lbu     t1, 2(t0)
    li      t2, 0x53    # 'S'
    bne     t1, t2, fail_content
    lbu     t1, 3(t0)
    li      t2, 0x4D    # 'M'
    bne     t1, t2, fail_content
    lbu     t1, 4(t0)
    li      t2, 0x4F    # 'O'
    bne     t1, t2, fail_content

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

fail_opcode:
    li      gp, 7
    li      a0, 1
    ecall

fail_block:
    li      gp, 9
    li      a0, 1
    ecall

fail_content:
    li      gp, 11
    li      a0, 1
    ecall
