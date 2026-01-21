# TFTP Read/Write Test
# 1. Read data/readme.txt, verify "COSMO"
# 2. Write data/rw_test.txt with "TESTOK\n"
# 3. Read back, verify content

.section .text
.globl _start

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

    # Setup MAC
    li      t0, ETH_MACA0HR
    li      t1, 0x0002
    sw      t1, 0(t0)
    li      t0, ETH_MACA0LR
    li      t1, 0x03040506
    sw      t1, 0(t0)

    # Enable
    li      t0, ETH_MACCR
    li      t1, MACCR_TE | MACCR_RE
    sw      t1, 0(t0)
    li      t0, ETH_DMAOMR
    li      t1, DMAOMR_ST | DMAOMR_SR
    sw      t1, 0(t0)

# ========== TEST 1: Read data/readme.txt ==========
test1_read:
    jal     build_eth_ip_hdr    # Build common header

    # UDP src port 1111 (0x0457)
    li      t0, TX_BUF + 34
    li      t1, 0x04
    sb      t1, 0(t0)
    li      t1, 0x57
    sb      t1, 1(t0)
    # Dst port 69
    sb      zero, 2(t0)
    li      t1, 69
    sb      t1, 3(t0)
    # UDP len: 8 + 2 + 16 + 6 = 32
    sb      zero, 4(t0)
    li      t1, 32
    sb      t1, 5(t0)
    sh      zero, 6(t0)

    # RRQ opcode=1
    li      t0, TX_BUF + 42
    sb      zero, 0(t0)
    li      t1, 1
    sb      t1, 1(t0)

    # "data/readme.txt\0" = 16 bytes
    la      a0, fn_readme
    li      a1, TX_BUF + 44
    jal     strcpy

    # "octet\0" = 6 bytes
    la      a0, mode_octet
    li      a1, TX_BUF + 60
    jal     strcpy

    # IP total = 20 + 32 = 52
    li      t0, TX_BUF + 16
    sb      zero, 0(t0)
    li      t1, 52
    sb      t1, 1(t0)

    li      a0, 66              # 14 + 52
    jal     send_and_wait

    beqz    a0, fail_rx1

    # Verify DATA opcode=3, block=1
    li      t0, RX_BUF + 42
    lbu     t1, 1(t0)
    li      t2, 3
    bne     t1, t2, fail_op1
    lbu     t1, 3(t0)
    li      t2, 1
    bne     t1, t2, fail_blk1

    # Verify "COSMO"
    li      t0, RX_BUF + 46
    lbu     t1, 0(t0)
    li      t2, 'C'
    bne     t1, t2, fail_data1
    lbu     t1, 1(t0)
    li      t2, 'O'
    bne     t1, t2, fail_data1

# ========== TEST 2: Write data/rw_test.txt ==========
test2_write:
    jal     build_eth_ip_hdr

    # UDP src port 2222 (0x08AE)
    li      t0, TX_BUF + 34
    li      t1, 0x08
    sb      t1, 0(t0)
    li      t1, 0xAE
    sb      t1, 1(t0)
    sb      zero, 2(t0)
    li      t1, 69
    sb      t1, 3(t0)
    # UDP len: 8 + 2 + 16 + 6 = 32
    sb      zero, 4(t0)
    li      t1, 32
    sb      t1, 5(t0)
    sh      zero, 6(t0)

    # WRQ opcode=2
    li      t0, TX_BUF + 42
    sb      zero, 0(t0)
    li      t1, 2
    sb      t1, 1(t0)

    # "data/rw_test.txt\0" = 17 bytes
    la      a0, fn_rwtest
    li      a1, TX_BUF + 44
    jal     strcpy

    la      a0, mode_octet
    li      a1, TX_BUF + 61
    jal     strcpy

    # IP total = 20 + 33 = 53
    li      t0, TX_BUF + 16
    sb      zero, 0(t0)
    li      t1, 53
    sb      t1, 1(t0)

    li      a0, 67
    jal     send_and_wait

    beqz    a0, fail_rx2

    # Verify ACK 0
    li      t0, RX_BUF + 42
    lbu     t1, 1(t0)
    li      t2, 4
    bne     t1, t2, fail_op2
    lbu     t1, 3(t0)
    bnez    t1, fail_blk2

    # Send DATA block 1 with "TESTOK\n"
    jal     build_eth_ip_hdr

    li      t0, TX_BUF + 34
    li      t1, 0x08
    sb      t1, 0(t0)
    li      t1, 0xAE
    sb      t1, 1(t0)
    sb      zero, 2(t0)
    li      t1, 69
    sb      t1, 3(t0)
    # UDP len: 8 + 4 + 7 = 19
    sb      zero, 4(t0)
    li      t1, 19
    sb      t1, 5(t0)
    sh      zero, 6(t0)

    # DATA opcode=3, block=1
    li      t0, TX_BUF + 42
    sb      zero, 0(t0)
    li      t1, 3
    sb      t1, 1(t0)
    sb      zero, 2(t0)
    li      t1, 1
    sb      t1, 3(t0)

    # "TESTOK\n"
    li      t0, TX_BUF + 46
    li      t1, 'T'
    sb      t1, 0(t0)
    li      t1, 'E'
    sb      t1, 1(t0)
    li      t1, 'S'
    sb      t1, 2(t0)
    li      t1, 'T'
    sb      t1, 3(t0)
    li      t1, 'O'
    sb      t1, 4(t0)
    li      t1, 'K'
    sb      t1, 5(t0)
    li      t1, '\n'
    sb      t1, 6(t0)

    # IP total = 20 + 19 = 39
    li      t0, TX_BUF + 16
    sb      zero, 0(t0)
    li      t1, 39
    sb      t1, 1(t0)

    li      a0, 53              # 14 + 39
    jal     send_and_wait

    beqz    a0, fail_rx3

    # Verify ACK 1
    li      t0, RX_BUF + 42
    lbu     t1, 1(t0)
    li      t2, 4
    bne     t1, t2, fail_op3
    lbu     t1, 3(t0)
    li      t2, 1
    bne     t1, t2, fail_blk3

# ========== TEST 3: Read back written file ==========
test3_readback:
    jal     build_eth_ip_hdr

    # UDP src port 3333 (0x0D05)
    li      t0, TX_BUF + 34
    li      t1, 0x0D
    sb      t1, 0(t0)
    li      t1, 0x05
    sb      t1, 1(t0)
    sb      zero, 2(t0)
    li      t1, 69
    sb      t1, 3(t0)
    # UDP len: 8 + 2 + 17 + 6 = 33
    sb      zero, 4(t0)
    li      t1, 33
    sb      t1, 5(t0)
    sh      zero, 6(t0)

    # RRQ opcode=1
    li      t0, TX_BUF + 42
    sb      zero, 0(t0)
    li      t1, 1
    sb      t1, 1(t0)

    la      a0, fn_rwtest
    li      a1, TX_BUF + 44
    jal     strcpy

    la      a0, mode_octet
    li      a1, TX_BUF + 61
    jal     strcpy

    # IP total = 20 + 33 = 53
    li      t0, TX_BUF + 16
    sb      zero, 0(t0)
    li      t1, 53
    sb      t1, 1(t0)

    li      a0, 67
    jal     send_and_wait

    beqz    a0, fail_rx4

    # Verify DATA block 1
    li      t0, RX_BUF + 42
    lbu     t1, 1(t0)
    li      t2, 3
    bne     t1, t2, fail_op4

    # Verify "TESTOK"
    li      t0, RX_BUF + 46
    lbu     t1, 0(t0)
    li      t2, 'T'
    bne     t1, t2, fail_data2
    lbu     t1, 1(t0)
    li      t2, 'E'
    bne     t1, t2, fail_data2
    lbu     t1, 2(t0)
    li      t2, 'S'
    bne     t1, t2, fail_data2
    lbu     t1, 3(t0)
    li      t2, 'T'
    bne     t1, t2, fail_data2
    lbu     t1, 4(t0)
    li      t2, 'O'
    bne     t1, t2, fail_data2
    lbu     t1, 5(t0)
    li      t2, 'K'
    bne     t1, t2, fail_data2

pass:
    li      gp, 1
    li      a0, 0
    ecall

fail_rx1:
    li      gp, 3
    j       fail
fail_op1:
    li      gp, 5
    j       fail
fail_blk1:
    li      gp, 7
    j       fail
fail_data1:
    li      gp, 9
    j       fail
fail_rx2:
    li      gp, 11
    j       fail
fail_op2:
    li      gp, 13
    j       fail
fail_blk2:
    li      gp, 15
    j       fail
fail_rx3:
    li      gp, 17
    j       fail
fail_op3:
    li      gp, 19
    j       fail
fail_blk3:
    li      gp, 21
    j       fail
fail_rx4:
    li      gp, 23
    j       fail
fail_op4:
    li      gp, 25
    j       fail
fail_data2:
    li      gp, 27
    j       fail
fail:
    li      a0, 1
    ecall

# ========== Helper routines ==========

# Build ETH + IP header (fixed parts)
build_eth_ip_hdr:
    li      t0, TX_BUF
    # Dest MAC: 02:00:00:00:00:01
    li      t1, 0x02
    sb      t1, 0(t0)
    sb      zero, 1(t0)
    sb      zero, 2(t0)
    sb      zero, 3(t0)
    sb      zero, 4(t0)
    li      t1, 0x01
    sb      t1, 5(t0)
    # Src MAC
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
    # EtherType
    li      t1, 0x08
    sb      t1, 12(t0)
    sb      zero, 13(t0)
    # IP
    li      t1, 0x45
    sb      t1, 14(t0)
    sb      zero, 15(t0)
    # len filled by caller
    sh      zero, 18(t0)
    sh      zero, 20(t0)
    li      t1, 64
    sb      t1, 22(t0)
    li      t1, 17
    sb      t1, 23(t0)
    sh      zero, 24(t0)
    # Src: 10.0.0.2
    li      t1, 10
    sb      t1, 26(t0)
    sb      zero, 27(t0)
    sb      zero, 28(t0)
    li      t1, 2
    sb      t1, 29(t0)
    # Dst: 10.0.0.1
    li      t1, 10
    sb      t1, 30(t0)
    sb      zero, 31(t0)
    sb      zero, 32(t0)
    li      t1, 1
    sb      t1, 33(t0)
    ret

# strcpy: a0=src, a1=dst (includes null)
strcpy:
    lbu     t0, 0(a0)
    sb      t0, 0(a1)
    addi    a0, a0, 1
    addi    a1, a1, 1
    bnez    t0, strcpy
    ret

# send_and_wait: a0=packet_len, returns a0=1 if rx ok, 0 if timeout
send_and_wait:
    mv      t5, a0

    # TX descriptor
    li      t0, TX_DESC
    li      t1, TDES0_OWN | TDES0_FS | TDES0_LS | TDES0_TCH
    sw      t1, 0(t0)
    sw      t5, 4(t0)
    li      t1, TX_BUF
    sw      t1, 8(t0)
    li      t1, TX_DESC
    sw      t1, 12(t0)

    # RX descriptor
    li      t0, RX_DESC
    li      t1, RDES0_OWN
    sw      t1, 0(t0)
    li      t1, 1024 | RDES1_RCH
    sw      t1, 4(t0)
    li      t1, RX_BUF
    sw      t1, 8(t0)
    li      t1, RX_DESC
    sw      t1, 12(t0)

    li      t0, ETH_DMATDLAR
    li      t1, TX_DESC
    sw      t1, 0(t0)
    li      t0, ETH_DMARDLAR
    li      t1, RX_DESC
    sw      t1, 0(t0)

    # TX poll
    li      t0, ETH_DMATPDR
    sw      zero, 0(t0)

    # Wait TX
    li      t0, ETH_DMASR
    li      t3, 100000
.Lwait_tx:
    lw      t1, 0(t0)
    andi    t2, t1, DMASR_TS
    bnez    t2, .Ltx_ok
    addi    t3, t3, -1
    bnez    t3, .Lwait_tx
    li      a0, 0
    ret
.Ltx_ok:
    li      t1, DMASR_TS
    sw      t1, 0(t0)

    # Wait RX
    li      t3, 100000
.Lwait_rx:
    lw      t1, 0(t0)
    andi    t2, t1, DMASR_RS
    bnez    t2, .Lrx_ok
    addi    t3, t3, -1
    bnez    t3, .Lwait_rx
    li      a0, 0
    ret
.Lrx_ok:
    li      t1, DMASR_RS
    sw      t1, 0(t0)
    li      a0, 1
    ret

# ========== Data ==========
.section .rodata
fn_readme:
    .asciz "data/readme.txt"
fn_rwtest:
    .asciz "data/rw_test.txt"
mode_octet:
    .asciz "octet"
