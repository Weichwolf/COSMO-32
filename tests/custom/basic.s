# Basic CPU test
# PASS: gp(x3) = 1, a0(x10) = 0

.section .text
.globl _start

_start:
    # Test ADDI
    li      t0, 5           # t0 = 5
    addi    t1, t0, 3       # t1 = 8

    # Test ADD
    add     t2, t0, t1      # t2 = 13

    # Test SUB
    sub     t3, t2, t0      # t3 = 8

    # Verify t3 == t1
    bne     t3, t1, fail

    # Test LUI/AUIPC
    lui     t4, 0x12345     # t4 = 0x12345000
    srli    t4, t4, 12      # t4 = 0x12345
    li      t5, 0x12345
    bne     t4, t5, fail

    # Test shifts
    li      t0, 1
    slli    t0, t0, 4       # t0 = 16
    li      t1, 16
    bne     t0, t1, fail

    # Test AND/OR/XOR
    li      t0, 0xFF
    li      t1, 0x0F
    and     t2, t0, t1      # t2 = 0x0F
    bne     t2, t1, fail

    or      t3, t0, t1      # t3 = 0xFF
    bne     t3, t0, fail

    xor     t4, t0, t1      # t4 = 0xF0
    li      t5, 0xF0
    bne     t4, t5, fail

    # Test memory (store/load)
    la      a1, test_data
    li      t0, 0xDEADBEEF
    sw      t0, 0(a1)
    lw      t1, 0(a1)
    bne     t0, t1, fail

    # Test byte load/store
    li      t0, 0x42
    sb      t0, 4(a1)
    lbu     t1, 4(a1)
    bne     t0, t1, fail

    # All tests passed
pass:
    li      gp, 1           # gp = 1 (test infrastructure marker)
    li      a0, 0           # a0 = 0 (success)
    ecall

fail:
    li      gp, 1           # gp = 1
    li      a0, 1           # a0 = 1 (failure)
    ecall

.section .data
test_data:
    .word 0
    .word 0
