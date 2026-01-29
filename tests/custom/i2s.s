# I2S Audio peripheral test

.section .text
.globl _start

# I2S registers (at 0x40013000)
.equ I2S_BASE,      0x40013000
.equ I2S_CTRL,      0x40013000  # Control
.equ I2S_STATUS,    0x40013004  # Status
.equ I2S_DATA,      0x40013008  # Data (write samples)
.equ I2S_CLKDIV,    0x4001300C  # Clock divider
.equ I2S_BUFCNT,    0x40013010  # Buffer count

# CTRL bits
.equ CTRL_EN,       (1 << 0)    # Enable
.equ CTRL_TXIE,     (1 << 1)    # TX empty IRQ enable
.equ CTRL_DMAE,     (1 << 2)    # DMA enable
.equ CTRL_STEREO,   (1 << 3)    # Stereo mode
.equ CTRL_FMT16,    (1 << 4)    # 16-bit format

# STATUS bits
.equ STATUS_TXE,    (1 << 0)    # TX empty
.equ STATUS_TXNF,   (1 << 1)    # TX not full
.equ STATUS_TXHF,   (1 << 2)    # TX half full
.equ STATUS_BSY,    (1 << 3)    # Busy

_start:
    # Initialize stack
    lui     sp, 0x20010

    # Test 1: Read default status (should have TXE and TXNF set)
    li      t0, I2S_STATUS
    lw      t1, 0(t0)
    li      t2, STATUS_TXE | STATUS_TXNF
    and     t3, t1, t2
    bne     t3, t2, fail1

    # Test 2: Read default clock divider
    li      t0, I2S_CLKDIV
    lw      t1, 0(t0)
    beqz    t1, fail2       # Should be non-zero (default 22050 Hz)

    # Test 3: Set clock divider for 44100 Hz
    # clkdiv = 144000000 / 44100 = 3265
    li      t0, I2S_CLKDIV
    li      t1, 3265
    sw      t1, 0(t0)
    lw      t2, 0(t0)
    bne     t1, t2, fail3

    # Test 4: Enable I2S in stereo mode
    li      t0, I2S_CTRL
    li      t1, CTRL_EN | CTRL_STEREO | CTRL_FMT16
    sw      t1, 0(t0)
    lw      t2, 0(t0)
    bne     t1, t2, fail4

    # Test 5: Write stereo sample (left | right<<16)
    li      t0, I2S_DATA
    li      t1, 0x1234      # Left channel
    li      t2, 0x5678      # Right channel
    slli    t2, t2, 16
    or      t1, t1, t2      # Combined stereo sample
    sw      t1, 0(t0)

    # Test 6: Check buffer count increased
    li      t0, I2S_BUFCNT
    lw      t1, 0(t0)
    beqz    t1, fail6       # Should be 1

    # Test 7: Write more samples
    li      t0, I2S_DATA
    li      t3, 10          # Write 10 more samples
write_loop:
    li      t1, 0xABCD0000
    or      t1, t1, t3      # Sample varies with counter
    sw      t1, 0(t0)
    addi    t3, t3, -1
    bnez    t3, write_loop

    # Test 8: Check buffer count
    li      t0, I2S_BUFCNT
    lw      t1, 0(t0)
    li      t2, 11          # Should be 11 samples now
    bne     t1, t2, fail8

    # Test 9: Check status (should not be empty anymore)
    li      t0, I2S_STATUS
    lw      t1, 0(t0)
    li      t2, STATUS_TXE
    and     t3, t1, t2
    bnez    t3, fail9       # TXE should be clear (not empty)

    # Test 10: Disable I2S (should reset buffer)
    li      t0, I2S_CTRL
    sw      zero, 0(t0)

    # Test 11: Check buffer cleared
    li      t0, I2S_BUFCNT
    lw      t1, 0(t0)
    bnez    t1, fail11      # Should be 0 after disable

    # All tests passed
pass:
    li      gp, 1
    li      a0, 0
    ecall

fail1:
    li      gp, 3
    li      a0, 1
    ecall

fail2:
    li      gp, 5
    li      a0, 1
    ecall

fail3:
    li      gp, 7
    li      a0, 1
    ecall

fail4:
    li      gp, 9
    li      a0, 1
    ecall

fail6:
    li      gp, 13
    li      a0, 1
    ecall

fail8:
    li      gp, 17
    li      a0, 1
    ecall

fail9:
    li      gp, 19
    li      a0, 1
    ecall

fail11:
    li      gp, 23
    li      a0, 1
    ecall
