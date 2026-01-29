# Branch instruction tests

.section .text
.globl _start

_start:
    # BEQ - equal
    li      t0, 5
    li      t1, 5
    beq     t0, t1, beq_ok
    j       fail
beq_ok:

    # BEQ - not equal (should not branch)
    li      t1, 6
    beq     t0, t1, fail

    # BNE - not equal
    bne     t0, t1, bne_ok
    j       fail
bne_ok:

    # BNE - equal (should not branch)
    li      t1, 5
    bne     t0, t1, fail

    # BLT - signed less than
    li      t0, -5
    li      t1, 5
    blt     t0, t1, blt_ok
    j       fail
blt_ok:

    # BLT - not less (should not branch)
    blt     t1, t0, fail

    # BGE - signed greater or equal
    li      t0, 5
    li      t1, 5
    bge     t0, t1, bge_ok1
    j       fail
bge_ok1:
    li      t0, 6
    bge     t0, t1, bge_ok2
    j       fail
bge_ok2:

    # BLTU - unsigned less than
    li      t0, 5
    li      t1, -1          # 0xFFFFFFFF (largest unsigned)
    bltu    t0, t1, bltu_ok
    j       fail
bltu_ok:

    # BGEU - unsigned greater or equal
    bgeu    t1, t0, bgeu_ok
    j       fail
bgeu_ok:

    # JAL
    jal     ra, jal_target
    j       after_jal
jal_target:
    # Check ra was set correctly
    # ra should point to instruction after jal
    jalr    zero, ra, 0     # Return
after_jal:

    # JALR
    la      t0, jalr_target
    jalr    ra, t0, 0
    j       after_jalr
jalr_target:
    jalr    zero, ra, 0
after_jalr:

pass:
    li      gp, 1
    li      a0, 0
    ecall

fail:
    li      gp, 1
    li      a0, 1
    ecall
