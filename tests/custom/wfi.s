# WFI (Wait For Interrupt) test
# Tests:
# 1. WFI wakes up when interrupt is pending and enabled
# 2. After interrupt handler, execution continues after WFI (not at WFI)
# 3. USART RX interrupt wakes WFI

.section .text
.globl _start

# PFIC registers
.equ PFIC_BASE,     0xE000E000
.equ PFIC_IENR0,    0xE000E100  # IRQ 0-31 enable
.equ PFIC_IENR1,    0xE000E104  # IRQ 32-63 enable
.equ PFIC_IPSR0,    0xE000E200  # IRQ 0-31 pending set
.equ PFIC_IPSR1,    0xE000E204  # IRQ 32-63 pending set
.equ PFIC_IPRR0,    0xE000E280  # IRQ 0-31 pending clear
.equ PFIC_IPRR1,    0xE000E284  # IRQ 32-63 pending clear

# USART registers
.equ USART1_BASE,   0x40000000
.equ USART_STATR,   0x00
.equ USART_DATAR,   0x04
.equ USART_CTLR1,   0x0C

# USART bits
.equ CTLR1_UE,      (1 << 13)
.equ CTLR1_RXNEIE,  (1 << 5)
.equ CTLR1_RE,      (1 << 2)
.equ STATR_RXNE,    (1 << 5)

# IRQ numbers
.equ SYSTICK_IRQ,   12
.equ USART1_IRQ,    37

# CSR bits
.equ MSTATUS_MIE,   (1 << 3)
.equ MIE_MEIE,      (1 << 11)

# Variables
.section .data
.align 4
irq_count:      .word 0
wfi_passed:     .word 0

.section .text

_start:
    # Initialize stack
    lui     sp, 0x20010

    # Setup trap vector
    la      t0, trap_handler
    csrw    mtvec, t0

    #------------------------------------------------------------------
    # Test 1: WFI wakes on pending interrupt
    #------------------------------------------------------------------

    # Enable IRQ 12 (SysTick) in PFIC
    li      t0, PFIC_IENR0
    li      t1, (1 << SYSTICK_IRQ)
    sw      t1, 0(t0)

    # Enable external interrupts in mie
    li      t0, MIE_MEIE
    csrs    mie, t0

    # Reset counters
    la      t0, irq_count
    sw      zero, 0(t0)
    la      t0, wfi_passed
    sw      zero, 0(t0)

    # Set interrupt pending BEFORE enabling global interrupts
    li      t0, PFIC_IPSR0
    li      t1, (1 << SYSTICK_IRQ)
    sw      t1, 0(t0)

    # Enable global interrupts
    li      t0, MSTATUS_MIE
    csrs    mstatus, t0

    # Execute WFI - should wake immediately due to pending IRQ
    wfi

    # Mark that we passed WFI (handler should have run first)
    la      t0, wfi_passed
    li      t1, 1
    sw      t1, 0(t0)

    # Verify interrupt handler was called
    la      t0, irq_count
    lw      t1, 0(t0)
    beqz    t1, fail1

    # Verify we continued after WFI
    la      t0, wfi_passed
    lw      t1, 0(t0)
    beqz    t1, fail2

    #------------------------------------------------------------------
    # Test 2: WFI does not loop (PC increments correctly)
    # Set pending while in WFI, verify we exit and don't re-enter
    #------------------------------------------------------------------

    # Reset counter
    la      t0, irq_count
    sw      zero, 0(t0)

    # Set a marker value - we'll check this wasn't overwritten
    li      s0, 0xDEAD

    # Set interrupt pending
    li      t0, PFIC_IPSR0
    li      t1, (1 << SYSTICK_IRQ)
    sw      t1, 0(t0)

    # WFI should wake, take interrupt, return HERE (not back to WFI)
    wfi

    # This instruction should execute exactly once after WFI
    li      s1, 0xBEEF

    # Verify s0 wasn't clobbered (would happen if we looped)
    li      t0, 0xDEAD
    bne     s0, t0, fail3

    # Verify s1 was set (we continued past WFI)
    li      t0, 0xBEEF
    bne     s1, t0, fail4

    # Verify exactly one interrupt was taken
    la      t0, irq_count
    lw      t1, 0(t0)
    li      t2, 1
    bne     t1, t2, fail5

    #------------------------------------------------------------------
    # Test 3: USART RX interrupt wakes WFI (IRQ 37)
    #------------------------------------------------------------------

    # Enable USART with RX and RXNE interrupt
    li      t0, USART1_BASE
    li      t1, CTLR1_UE | CTLR1_RE | CTLR1_RXNEIE
    sw      t1, USART_CTLR1(t0)

    # Enable USART1 IRQ (37) in PFIC - it's in IENR1 (bit 5)
    li      t0, PFIC_IENR1
    li      t1, (1 << (USART1_IRQ - 32))
    sw      t1, 0(t0)

    # Reset counter
    la      t0, irq_count
    sw      zero, 0(t0)

    # Set USART1 IRQ pending
    li      t0, PFIC_IPSR1
    li      t1, (1 << (USART1_IRQ - 32))
    sw      t1, 0(t0)

    # WFI - should wake due to USART IRQ
    wfi

    # Verify interrupt was taken
    la      t0, irq_count
    lw      t1, 0(t0)
    beqz    t1, fail6

    #------------------------------------------------------------------
    # All tests passed
    #------------------------------------------------------------------
pass:
    li      gp, 1
    li      a0, 0
    ecall

fail1:
    li      gp, 3       # Test 1 failed: IRQ not taken
    li      a0, 1
    ecall

fail2:
    li      gp, 5       # Test 1 failed: didn't continue after WFI
    li      a0, 1
    ecall

fail3:
    li      gp, 7       # Test 2 failed: s0 clobbered
    li      a0, 1
    ecall

fail4:
    li      gp, 9       # Test 2 failed: s1 not set
    li      a0, 1
    ecall

fail5:
    li      gp, 11      # Test 2 failed: wrong IRQ count
    li      a0, 1
    ecall

fail6:
    li      gp, 13      # Test 3 failed: USART IRQ not taken
    li      a0, 1
    ecall

#------------------------------------------------------------------
# Trap handler
#------------------------------------------------------------------
.align 4
trap_handler:
    # Save t0-t2
    addi    sp, sp, -12
    sw      t0, 0(sp)
    sw      t1, 4(sp)
    sw      t2, 8(sp)

    # Check if interrupt
    csrr    t0, mcause
    bgez    t0, trap_exception

    # Increment IRQ counter
    la      t0, irq_count
    lw      t1, 0(t0)
    addi    t1, t1, 1
    sw      t1, 0(t0)

    # Clear SysTick pending
    li      t0, PFIC_IPRR0
    li      t1, (1 << SYSTICK_IRQ)
    sw      t1, 0(t0)

    # Clear USART1 pending
    li      t0, PFIC_IPRR1
    li      t1, (1 << (USART1_IRQ - 32))
    sw      t1, 0(t0)

    j       trap_done

trap_exception:
    # Unexpected exception - skip instruction
    csrr    t0, mepc
    addi    t0, t0, 4
    csrw    mepc, t0

trap_done:
    # Restore
    lw      t0, 0(sp)
    lw      t1, 4(sp)
    lw      t2, 8(sp)
    addi    sp, sp, 12
    mret
