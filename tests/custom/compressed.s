# RV32C Compressed instruction tests
# C-extension uses 3-bit register encoding for some instructions:
# rd'/rs1'/rs2' maps to x8-x15 (s0, s1, a0-a5)

.option rvc

.section .text
.globl _start

_start:
    # Initialize stack pointer
    lui     sp, 0x20010     # sp = 0x20010000 (top of 64KB SRAM)

    # C.LI - load immediate (6-bit signed: -32 to 31)
    c.li    a0, 10
    li      t1, 10
    bne     a0, t1, fail

    c.li    a1, -5
    li      t1, -5
    bne     a1, t1, fail

    # C.MV (any rd != 0, any rs2 != 0)
    li      a2, 42
    c.mv    a3, a2
    bne     a2, a3, fail

    # C.ADD
    c.li    a0, 10
    c.li    a1, 20
    c.add   a0, a1
    li      t0, 30
    bne     a0, t0, fail

    # C.ADDI
    c.li    a0, 5
    c.addi  a0, 3
    li      t0, 8
    bne     a0, t0, fail

    # C.SLLI
    c.li    a0, 1
    c.slli  a0, 4
    li      t0, 16
    bne     a0, t0, fail

    # C.SRLI (rd' restricted to x8-x15)
    li      s0, 32
    c.srli  s0, 2
    li      t0, 8
    bne     s0, t0, fail

    # C.SRAI
    li      s0, -32
    c.srai  s0, 2
    li      t0, -8
    bne     s0, t0, fail

    # C.ANDI
    li      s1, 0xFF
    c.andi  s1, 0x0F
    li      t0, 0x0F
    bne     s1, t0, fail

    # C.SUB
    li      s0, 20
    li      s1, 8
    c.sub   s0, s1
    li      t0, 12
    bne     s0, t0, fail

    # C.XOR
    li      s0, 0xFF
    li      s1, 0x0F
    c.xor   s0, s1
    li      t0, 0xF0
    bne     s0, t0, fail

    # C.OR
    li      s0, 0xF0
    li      s1, 0x0F
    c.or    s0, s1
    li      t0, 0xFF
    bne     s0, t0, fail

    # C.AND
    li      s0, 0xFF
    li      s1, 0xF0
    c.and   s0, s1
    li      t0, 0xF0
    bne     s0, t0, fail

    # C.J
    c.j     cj_ok
    j       fail
cj_ok:

    # C.BEQZ
    li      s0, 0
    c.beqz  s0, beqz_ok
    j       fail
beqz_ok:

    # C.BNEZ
    c.li    s1, 1
    c.bnez  s1, bnez_ok
    j       fail
bnez_ok:

    # C.LWSP / C.SWSP
    addi    sp, sp, -16
    li      t0, 0x12345678
    c.swsp  t0, 0(sp)
    c.lwsp  t1, 0(sp)
    addi    sp, sp, 16
    bne     t0, t1, fail

    # C.LW / C.SW (rs1' and rd'/rs2' must be x8-x15)
    la      t0, test_data
    mv      s0, t0
    li      s1, 0xCAFEBABE
    c.sw    s1, 0(s0)
    c.lw    a0, 0(s0)
    bne     s1, a0, fail

    # C.JAL (RV32 only)
    c.jal   jal_target
    j       after_jal
jal_target:
    c.jr    ra
after_jal:

    # C.JALR
    la      t0, jalr_target
    c.jalr  t0
    j       after_jalr
jalr_target:
    c.jr    ra
after_jalr:

    # C.ADDI4SPN
    addi    sp, sp, -32
    c.addi4spn s0, sp, 8
    addi    t0, sp, 8
    bne     s0, t0, fail
    addi    sp, sp, 32

    # C.ADDI16SP
    mv      t0, sp
    c.addi16sp sp, 32
    addi    t1, t0, 32
    bne     sp, t1, fail
    c.addi16sp sp, -32

    # C.LUI
    c.lui   a0, 0x12
    lui     t0, 0x12
    bne     a0, t0, fail

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
