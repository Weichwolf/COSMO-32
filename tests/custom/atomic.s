# RV32A Atomic instruction tests

.section .text
.globl _start

_start:
    la      a0, test_data

    # AMOSWAP.W
    li      t0, 42
    sw      t0, 0(a0)
    li      t1, 100
    amoswap.w t2, t1, (a0)  # t2 = old value (42), mem = 100
    li      t3, 42
    bne     t2, t3, fail
    lw      t4, 0(a0)
    li      t3, 100
    bne     t4, t3, fail

    # AMOADD.W
    li      t0, 50
    sw      t0, 0(a0)
    li      t1, 25
    amoadd.w t2, t1, (a0)   # t2 = 50, mem = 75
    li      t3, 50
    bne     t2, t3, fail
    lw      t4, 0(a0)
    li      t3, 75
    bne     t4, t3, fail

    # AMOXOR.W
    li      t0, 0xFF
    sw      t0, 0(a0)
    li      t1, 0x0F
    amoxor.w t2, t1, (a0)   # t2 = 0xFF, mem = 0xF0
    li      t3, 0xFF
    bne     t2, t3, fail
    lw      t4, 0(a0)
    li      t3, 0xF0
    bne     t4, t3, fail

    # AMOAND.W
    li      t0, 0xFF
    sw      t0, 0(a0)
    li      t1, 0xF0
    amoand.w t2, t1, (a0)   # t2 = 0xFF, mem = 0xF0
    li      t3, 0xFF
    bne     t2, t3, fail
    lw      t4, 0(a0)
    li      t3, 0xF0
    bne     t4, t3, fail

    # AMOOR.W
    li      t0, 0xF0
    sw      t0, 0(a0)
    li      t1, 0x0F
    amoor.w t2, t1, (a0)    # t2 = 0xF0, mem = 0xFF
    li      t3, 0xF0
    bne     t2, t3, fail
    lw      t4, 0(a0)
    li      t3, 0xFF
    bne     t4, t3, fail

    # AMOMIN.W (signed)
    li      t0, 10
    sw      t0, 0(a0)
    li      t1, -5
    amomin.w t2, t1, (a0)   # t2 = 10, mem = -5 (smaller signed)
    li      t3, 10
    bne     t2, t3, fail
    lw      t4, 0(a0)
    li      t3, -5
    bne     t4, t3, fail

    # AMOMAX.W (signed)
    li      t0, -10
    sw      t0, 0(a0)
    li      t1, 5
    amomax.w t2, t1, (a0)   # t2 = -10, mem = 5 (larger signed)
    li      t3, -10
    bne     t2, t3, fail
    lw      t4, 0(a0)
    li      t3, 5
    bne     t4, t3, fail

    # AMOMINU.W (unsigned)
    li      t0, 100
    sw      t0, 0(a0)
    li      t1, 50
    amominu.w t2, t1, (a0)  # t2 = 100, mem = 50
    li      t3, 100
    bne     t2, t3, fail
    lw      t4, 0(a0)
    li      t3, 50
    bne     t4, t3, fail

    # AMOMAXU.W (unsigned)
    li      t0, 50
    sw      t0, 0(a0)
    li      t1, 100
    amomaxu.w t2, t1, (a0)  # t2 = 50, mem = 100
    li      t3, 50
    bne     t2, t3, fail
    lw      t4, 0(a0)
    li      t3, 100
    bne     t4, t3, fail

    # LR.W / SC.W
    li      t0, 42
    sw      t0, 0(a0)

    lr.w    t1, (a0)        # Load-reserved: t1 = 42
    li      t3, 42
    bne     t1, t3, fail

    addi    t2, t1, 8       # t2 = 50
    sc.w    t4, t2, (a0)    # Store-conditional: should succeed (t4 = 0)
    bnez    t4, fail        # t4 should be 0 on success

    lw      t5, 0(a0)
    li      t3, 50
    bne     t5, t3, fail

    # SC.W without LR should fail
    li      t1, 999
    sc.w    t4, t1, (a0)    # Should fail since no LR preceded
    beqz    t4, fail        # t4 should be 1 (failure)

pass:
    li      gp, 1
    li      a0, 0
    ecall

fail:
    li      gp, 1
    li      a0, 1
    ecall

.section .data
.align 4
test_data:
    .word 0
