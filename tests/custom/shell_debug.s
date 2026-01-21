# Shell debug test - verify shell startup

.section .text
.globl _start

.equ USART1_BASE,   0x40000000
.equ USART_STATR,   0x00
.equ USART_DATAR,   0x04
.equ USART_CTLR1,   0x0C
.equ STATR_TXE,     (1 << 7)
.equ CTLR1_UE,      (1 << 13)
.equ CTLR1_TE,      (1 << 3)
.equ CTLR1_RE,      (1 << 2)

.section .rodata
prompt:     .asciz "> "
msg_init:   .asciz "INIT\n"

.section .text

_start:
    lui     sp, 0x20010

    # Print "1"
    li      a0, '1'
    call    putchar

    # Init USART (same as shell_init)
    li      t0, USART1_BASE
    li      t1, CTLR1_UE | CTLR1_TE | CTLR1_RE
    sw      t1, USART_CTLR1(t0)

    # Print "2"
    li      a0, '2'
    call    putchar

    # Print "INIT\n"
    la      a0, msg_init
    call    print_str

    # Print "3"
    li      a0, '3'
    call    putchar

    # Print prompt
    la      a0, prompt
    call    print_str

    # Print "4"
    li      a0, '4'
    call    putchar

    # Newline and exit
    li      a0, '\n'
    call    putchar

    li      gp, 1
    li      a0, 0
    ecall

putchar:
    li      t0, USART1_BASE
1:  lw      t1, USART_STATR(t0)
    andi    t1, t1, STATR_TXE
    beqz    t1, 1b
    sw      a0, USART_DATAR(t0)
    ret

print_str:
    addi    sp, sp, -8
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    mv      s0, a0
1:  lbu     a0, 0(s0)
    beqz    a0, 2f
    call    putchar
    addi    s0, s0, 1
    j       1b
2:  lw      ra, 0(sp)
    lw      s0, 4(sp)
    addi    sp, sp, 8
    ret
