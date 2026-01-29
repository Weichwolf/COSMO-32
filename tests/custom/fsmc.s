# FSMC (External SRAM) test

.section .text
.globl _start

# FSMC addresses
.equ FSMC_BASE,     0x60000000
.equ FSMC_SIZE,     0x100000    # 1MB
.equ FB_OFFSET,     0xE0000     # Framebuffer at 896KB
.equ FB_BASE,       0x600E0000

_start:
    # Initialize stack
    lui     sp, 0x20010

    # Test 1: Write and read word at FSMC base
    li      t0, FSMC_BASE
    li      t1, 0xDEADBEEF
    sw      t1, 0(t0)
    lw      t2, 0(t0)
    bne     t1, t2, fail1

    # Test 2: Write and read at different offset
    li      t0, FSMC_BASE + 0x1000
    li      t1, 0xCAFEBABE
    sw      t1, 0(t0)
    lw      t2, 0(t0)
    bne     t1, t2, fail2

    # Test 3: Byte access
    li      t0, FSMC_BASE + 0x100
    li      t1, 0xAB
    sb      t1, 0(t0)
    lbu     t2, 0(t0)
    bne     t1, t2, fail3

    # Test 4: Halfword access
    li      t0, FSMC_BASE + 0x200
    li      t1, 0x1234
    sh      t1, 0(t0)
    lhu     t2, 0(t0)
    bne     t1, t2, fail4

    # Test 5: Write pattern to framebuffer area
    li      t0, FB_BASE
    li      t1, 0x12345678
    sw      t1, 0(t0)
    li      t1, 0x9ABCDEF0
    sw      t1, 4(t0)

    # Read back and verify
    lw      t2, 0(t0)
    li      t3, 0x12345678
    bne     t2, t3, fail5

    lw      t2, 4(t0)
    li      t3, 0x9ABCDEF0
    bne     t2, t3, fail5

    # Test 6: Write near end of FSMC (1MB - 4)
    li      t0, FSMC_BASE + FSMC_SIZE - 4
    li      t1, 0xFEEDFACE
    sw      t1, 0(t0)
    lw      t2, 0(t0)
    bne     t1, t2, fail6

    # Test 7: Verify first write wasn't corrupted
    li      t0, FSMC_BASE
    lw      t2, 0(t0)
    li      t1, 0xDEADBEEF
    bne     t1, t2, fail7

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

fail5:
    li      gp, 11
    li      a0, 1
    ecall

fail6:
    li      gp, 13
    li      a0, 1
    ecall

fail7:
    li      gp, 15
    li      a0, 1
    ecall
