# Simple USART test - print "X" and exit

.section .text
.globl _start

.equ USART1_BASE,   0x40000000
.equ USART_STATR,   0x00
.equ USART_DATAR,   0x04
.equ USART_CTLR1,   0x0C
.equ STATR_TXE,     (1 << 7)
.equ CTLR1_UE,      (1 << 13)
.equ CTLR1_TE,      (1 << 3)

_start:
    # Init stack
    lui     sp, 0x20010

    # Init USART
    li      t0, USART1_BASE
    li      t1, CTLR1_UE | CTLR1_TE
    sw      t1, USART_CTLR1(t0)

    # Wait for TX empty
1:  lw      t1, USART_STATR(t0)
    andi    t1, t1, STATR_TXE
    beqz    t1, 1b

    # Print 'X'
    li      t1, 'X'
    sw      t1, USART_DATAR(t0)

    # Wait for TX complete
2:  lw      t1, USART_STATR(t0)
    andi    t1, t1, STATR_TXE
    beqz    t1, 2b

    # Print newline
    li      t1, '\n'
    sw      t1, USART_DATAR(t0)

    # Wait
3:  lw      t1, USART_STATR(t0)
    andi    t1, t1, STATR_TXE
    beqz    t1, 3b

    # Exit
    li      gp, 1
    li      a0, 0
    ecall
