# RV32M Multiply/Divide test

.section .text
.globl _start

_start:
    # MUL
    li      t0, 7
    li      t1, 6
    mul     t2, t0, t1      # t2 = 42
    li      t3, 42
    bne     t2, t3, fail

    # MULH (signed high)
    li      t0, 0x7FFFFFFF
    li      t1, 2
    mulh    t2, t0, t1      # high bits of large product
    # Expected: 0x7FFFFFFF * 2 = 0xFFFFFFFE, high = 0
    # Actually high = 0 for this case
    bnez    t2, fail

    # MULHU (unsigned high)
    li      t0, -1          # 0xFFFFFFFF
    li      t1, 2
    mulhu   t2, t0, t1      # 0xFFFFFFFF * 2 = 1_FFFFFFFE, high = 1
    li      t3, 1
    bne     t2, t3, fail

    # DIV
    li      t0, 42
    li      t1, 7
    div     t2, t0, t1      # t2 = 6
    li      t3, 6
    bne     t2, t3, fail

    # DIVU
    li      t0, -14         # 0xFFFFFFF2
    li      t1, 7
    divu    t2, t0, t1      # Unsigned division
    # 0xFFFFFFF2 / 7 = big number
    # Just check it's not 0
    beqz    t2, fail

    # REM
    li      t0, 17
    li      t1, 5
    rem     t2, t0, t1      # t2 = 2
    li      t3, 2
    bne     t2, t3, fail

    # REMU
    li      t0, 17
    li      t1, 5
    remu    t2, t0, t1      # t2 = 2
    bne     t2, t3, fail

    # Division by zero
    li      t0, 42
    li      t1, 0
    div     t2, t0, t1      # Should return -1
    li      t3, -1
    bne     t2, t3, fail

    divu    t2, t0, t1      # Should return 0xFFFFFFFF
    bne     t2, t3, fail

pass:
    li      gp, 1
    li      a0, 0
    ecall

fail:
    li      gp, 1
    li      a0, 1
    ecall
