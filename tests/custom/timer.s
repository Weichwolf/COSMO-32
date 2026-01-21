# SysTick Timer peripheral test

.section .text
.globl _start

# SysTick registers (at 0xE0000000)
.equ SYSTICK_BASE,  0xE0000000
.equ SYSTICK_CTRL,  0xE0000000  # Control
.equ SYSTICK_SR,    0xE0000004  # Status
.equ SYSTICK_CNT_L, 0xE0000008  # Count low
.equ SYSTICK_CNT_H, 0xE000000C  # Count high
.equ SYSTICK_CMP_L, 0xE0000010  # Compare low
.equ SYSTICK_CMP_H, 0xE0000014  # Compare high
.equ SYSTICK_RELOAD,0xE0000018  # Reload value

# CTRL bits
.equ CTRL_ENABLE,   (1 << 0)
.equ CTRL_TICKINT,  (1 << 1)
.equ CTRL_MODE,     (1 << 2)    # 0=one-shot, 1=periodic

# SR bits
.equ SR_CNTIF,      (1 << 0)

_start:
    # Initialize stack
    lui     sp, 0x20010

    # Test 1: Write and read count register
    li      t0, SYSTICK_CNT_L
    li      t1, 0
    sw      t1, 0(t0)           # Reset count to 0
    lw      t2, 0(t0)
    bnez    t2, fail1           # Should be 0 (or very small after write)

    # Test 2: Write compare value
    li      t0, SYSTICK_CMP_L
    li      t1, 100             # Compare at 100 cycles
    sw      t1, 0(t0)
    lw      t2, 0(t0)
    bne     t1, t2, fail2

    # Test 3: Enable timer (one-shot mode)
    li      t0, SYSTICK_CNT_L
    li      t1, 0
    sw      t1, 0(t0)           # Reset count

    li      t0, SYSTICK_CTRL
    li      t1, CTRL_ENABLE     # Enable, no interrupt, one-shot
    sw      t1, 0(t0)

    # Wait for timer to count (just spin for a bit)
    li      t3, 200
wait_loop:
    addi    t3, t3, -1
    bnez    t3, wait_loop

    # Test 4: Check that count flag was set
    li      t0, SYSTICK_SR
    lw      t1, 0(t0)
    li      t2, SR_CNTIF
    and     t3, t1, t2
    beqz    t3, fail4           # CNTIF should be set

    # Test 5: Check that timer disabled itself (one-shot)
    li      t0, SYSTICK_CTRL
    lw      t1, 0(t0)
    li      t2, CTRL_ENABLE
    and     t3, t1, t2
    bnez    t3, fail5           # Enable bit should be cleared

    # Test 6: Clear status by writing
    li      t0, SYSTICK_SR
    li      t1, SR_CNTIF
    sw      t1, 0(t0)           # Write 1 to clear
    lw      t2, 0(t0)
    and     t3, t2, t1
    bnez    t3, fail6           # Should be cleared now

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

fail4:
    li      gp, 9
    li      a0, 1
    ecall

fail5:
    li      gp, 11
    li      a0, 1
    ecall

fail6:
    li      gp, 13
    li      a0, 1
    ecall
