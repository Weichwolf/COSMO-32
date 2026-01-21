# USART peripheral test

.section .text
.globl _start

# USART1 registers
.equ USART1_BASE,   0x40000000
.equ USART1_STATR,  0x40000000  # Status register
.equ USART1_DATAR,  0x40000004  # Data register
.equ USART1_BRR,    0x40000008  # Baud rate
.equ USART1_CTLR1,  0x4000000C  # Control register 1

# CTLR1 bits
.equ CTLR1_UE,      (1 << 13)   # USART enable
.equ CTLR1_TE,      (1 << 3)    # TX enable

# STATR bits
.equ STATR_TXE,     (1 << 7)    # TX empty

_start:
    # Initialize stack
    lui     sp, 0x20010

    # Test 1: Read status register (should have TXE set by default)
    li      t0, USART1_STATR
    lw      t1, 0(t0)
    li      t2, STATR_TXE
    and     t3, t1, t2
    beqz    t3, fail1       # TXE should be set

    # Test 2: Enable USART with TX
    li      t0, USART1_CTLR1
    li      t1, CTLR1_UE | CTLR1_TE
    sw      t1, 0(t0)

    # Verify it was written
    lw      t2, 0(t0)
    bne     t1, t2, fail2

    # Test 3: Write characters to DATAR
    li      t0, USART1_DATAR
    li      t1, 'O'
    sw      t1, 0(t0)
    li      t1, 'K'
    sw      t1, 0(t0)
    li      t1, '\n'
    sw      t1, 0(t0)

    # Test 4: Disable USART, verify TX doesn't work
    li      t0, USART1_CTLR1
    li      t1, 0
    sw      t1, 0(t0)

    # Write should be ignored when disabled
    li      t0, USART1_DATAR
    li      t1, 'X'
    sw      t1, 0(t0)       # This should not output

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
