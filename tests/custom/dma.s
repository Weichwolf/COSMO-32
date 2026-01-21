# DMA Controller test

.section .text
.globl _start

# DMA1 registers (at 0x40020000)
.equ DMA1_BASE,     0x40020000
.equ DMA1_ISR,      0x40020000  # Interrupt status
.equ DMA1_IFCR,     0x40020004  # Interrupt flag clear

# Channel 1 registers (offset 0x08, stride 0x14)
.equ DMA1_CH1_CCR,  0x40020008  # Control
.equ DMA1_CH1_CNDTR,0x4002000C  # Number of data
.equ DMA1_CH1_CPAR, 0x40020010  # Peripheral (source for M2M)
.equ DMA1_CH1_CMAR, 0x40020014  # Memory (dest for M2M)

# CCR bits
.equ CCR_EN,        (1 << 0)    # Enable
.equ CCR_TCIE,      (1 << 1)    # Transfer complete IRQ enable
.equ CCR_DIR,       (1 << 4)    # Direction
.equ CCR_PINC,      (1 << 6)    # Peripheral increment
.equ CCR_MINC,      (1 << 7)    # Memory increment
.equ CCR_PSIZE_W,   (2 << 8)    # Peripheral size = word
.equ CCR_MSIZE_W,   (2 << 10)   # Memory size = word
.equ CCR_MEM2MEM,   (1 << 14)   # Memory-to-memory mode

# ISR bits for channel 1
.equ ISR_GIF1,      (1 << 0)
.equ ISR_TCIF1,     (1 << 1)

# RAM addresses
.equ SRC_ADDR,      0x20008000
.equ DST_ADDR,      0x20009000

_start:
    # Initialize stack
    lui     sp, 0x20010

    # Test 1: Prepare source data in RAM
    li      t0, SRC_ADDR
    li      t1, 0xDEADBEEF
    sw      t1, 0(t0)
    li      t1, 0xCAFEBABE
    sw      t1, 4(t0)
    li      t1, 0x12345678
    sw      t1, 8(t0)
    li      t1, 0xABCDEF01
    sw      t1, 12(t0)

    # Clear destination
    li      t0, DST_ADDR
    sw      zero, 0(t0)
    sw      zero, 4(t0)
    sw      zero, 8(t0)
    sw      zero, 12(t0)

    # Test 2: Configure DMA channel 1 for memory-to-memory
    # Set source address (CPAR)
    li      t0, DMA1_CH1_CPAR
    li      t1, SRC_ADDR
    sw      t1, 0(t0)

    # Set destination address (CMAR)
    li      t0, DMA1_CH1_CMAR
    li      t1, DST_ADDR
    sw      t1, 0(t0)

    # Set transfer count (4 words)
    li      t0, DMA1_CH1_CNDTR
    li      t1, 4
    sw      t1, 0(t0)

    # Verify CNDTR was written
    lw      t2, 0(t0)
    li      t3, 4
    bne     t2, t3, fail2

    # Test 3: Start DMA transfer
    # CCR: Enable, MEM2MEM, PINC, MINC, word size
    li      t0, DMA1_CH1_CCR
    li      t1, CCR_EN | CCR_MEM2MEM | CCR_PINC | CCR_MINC | CCR_PSIZE_W | CCR_MSIZE_W
    sw      t1, 0(t0)

    # Wait for transfer to complete (poll ISR)
    li      t0, DMA1_ISR
    li      t3, 1000            # Timeout counter
wait_dma:
    lw      t1, 0(t0)
    li      t2, ISR_TCIF1
    and     t4, t1, t2
    bnez    t4, dma_done
    addi    t3, t3, -1
    bnez    t3, wait_dma
    j       fail3               # Timeout

dma_done:
    # Test 4: Verify transfer complete flag is set
    li      t0, DMA1_ISR
    lw      t1, 0(t0)
    li      t2, ISR_TCIF1 | ISR_GIF1
    and     t3, t1, t2
    beqz    t3, fail4

    # Test 5: Verify channel disabled itself (one-shot)
    li      t0, DMA1_CH1_CCR
    lw      t1, 0(t0)
    li      t2, CCR_EN
    and     t3, t1, t2
    bnez    t3, fail5           # EN should be cleared

    # Test 6: Verify destination data matches source
    li      t0, DST_ADDR
    li      t1, SRC_ADDR

    lw      t2, 0(t0)
    lw      t3, 0(t1)
    bne     t2, t3, fail6

    lw      t2, 4(t0)
    lw      t3, 4(t1)
    bne     t2, t3, fail6

    lw      t2, 8(t0)
    lw      t3, 8(t1)
    bne     t2, t3, fail6

    lw      t2, 12(t0)
    lw      t3, 12(t1)
    bne     t2, t3, fail6

    # Test 7: Clear ISR flags
    li      t0, DMA1_IFCR
    li      t1, ISR_TCIF1 | ISR_GIF1
    sw      t1, 0(t0)

    # Verify flags cleared
    li      t0, DMA1_ISR
    lw      t1, 0(t0)
    li      t2, ISR_TCIF1
    and     t3, t1, t2
    bnez    t3, fail7

    # All tests passed
pass:
    li      gp, 1
    li      a0, 0
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
