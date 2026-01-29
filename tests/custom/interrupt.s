# Interrupt handling test

.section .text
.globl _start

# PFIC registers (at 0xE000E000)
.equ PFIC_BASE,     0xE000E000
.equ PFIC_ISR0,     0xE000E000  # Interrupt status
.equ PFIC_IPR0,     0xE000E020  # Interrupt pending
.equ PFIC_ITHRESH,  0xE000E040  # Threshold
.equ PFIC_IENR0,    0xE000E100  # Interrupt enable set
.equ PFIC_IRER0,    0xE000E180  # Interrupt enable clear
.equ PFIC_IPSR0,    0xE000E200  # Interrupt pending set
.equ PFIC_IPRR0,    0xE000E280  # Interrupt pending clear

# SysTick IRQ number
.equ SYSTICK_IRQ,   12

# CSR addresses
.equ CSR_MSTATUS,   0x300
.equ CSR_MIE,       0x304
.equ CSR_MTVEC,     0x305
.equ CSR_MEPC,      0x341
.equ CSR_MCAUSE,    0x342
.equ CSR_MIP,       0x344

# mstatus bits
.equ MSTATUS_MIE,   (1 << 3)

# mie/mip bits
.equ MIE_MEIE,      (1 << 11)   # Machine external interrupt enable

# Variable to track interrupt execution
.section .data
.align 4
irq_count:
    .word 0

.section .text

_start:
    # Initialize stack
    lui     sp, 0x20010

    # Test 1: Setup trap vector (direct mode)
    la      t0, trap_handler
    csrw    mtvec, t0
    csrr    t1, mtvec
    # Mask off mode bits for comparison
    li      t2, ~3
    and     t0, t0, t2
    and     t1, t1, t2
    bne     t0, t1, fail1

    # Test 2: Enable external interrupts in PFIC
    li      t0, PFIC_IENR0
    li      t1, (1 << SYSTICK_IRQ)
    sw      t1, 0(t0)

    # Verify enabled
    li      t0, (PFIC_BASE + 0x100)  # IENR0
    lw      t2, 0(t0)
    and     t3, t2, t1
    beqz    t3, fail2

    # Test 3: Enable machine external interrupt in mie
    li      t0, MIE_MEIE
    csrs    mie, t0
    csrr    t1, mie
    and     t2, t1, t0
    beqz    t2, fail3

    # Test 4: Set interrupt pending in PFIC
    li      t0, PFIC_IPSR0
    li      t1, (1 << SYSTICK_IRQ)
    sw      t1, 0(t0)

    # Test 5: Set MIP.MEIE to signal external interrupt to CPU
    li      t0, MIE_MEIE
    csrs    mip, t0

    # Test 6: Enable global interrupts (this should trigger the interrupt)
    # First, reset irq_count
    la      t0, irq_count
    sw      zero, 0(t0)

    # Enable interrupts
    li      t0, MSTATUS_MIE
    csrs    mstatus, t0

    # NOP to allow interrupt to be taken
    nop
    nop
    nop

    # Test 7: Check that interrupt handler was called
    la      t0, irq_count
    lw      t1, 0(t0)
    beqz    t1, fail7       # Should have been incremented by handler

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

fail7:
    li      gp, 15
    li      a0, 1
    ecall

# Trap/Interrupt handler
.align 4
trap_handler:
    # Save registers
    addi    sp, sp, -16
    sw      t0, 0(sp)
    sw      t1, 4(sp)
    sw      t2, 8(sp)

    # Check if this is an interrupt (bit 31 of mcause set)
    csrr    t0, mcause
    bltz    t0, handle_interrupt
    # It's an exception - should not happen in this test
    j       trap_done

handle_interrupt:
    # Increment irq_count
    la      t0, irq_count
    lw      t1, 0(t0)
    addi    t1, t1, 1
    sw      t1, 0(t0)

    # Clear interrupt pending in PFIC
    li      t0, PFIC_IPRR0
    li      t1, (1 << SYSTICK_IRQ)
    sw      t1, 0(t0)

    # Clear MIP.MEIE
    li      t0, MIE_MEIE
    csrc    mip, t0

trap_done:
    # Restore registers
    lw      t0, 0(sp)
    lw      t1, 4(sp)
    lw      t2, 8(sp)
    addi    sp, sp, 16

    # Return from interrupt
    mret
