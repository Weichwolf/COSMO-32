# Display Control peripheral test

.section .text
.globl _start

# Display Control registers (at 0x40018000)
.equ DISPLAY_BASE,  0x40018000
.equ DISPLAY_MODE,  0x40018000  # Mode register
.equ DISPLAY_STATUS,0x40018004  # Status register
.equ DISPLAY_PAL,   0x40018040  # Palette base (16 x 16-bit)

# Mode values
.equ MODE_640x400,  0           # 4bpp with palette
.equ MODE_320x200,  1           # 16bpp direct

# Framebuffer in FSMC
.equ FB_BASE,       0x600E0000

_start:
    # Initialize stack
    lui     sp, 0x20010

    # Test 1: Read default mode (should be 0)
    li      t0, DISPLAY_MODE
    lw      t1, 0(t0)
    bnez    t1, fail1       # Default mode should be 0

    # Test 2: Set mode to 320x200x16bpp
    li      t0, DISPLAY_MODE
    li      t1, MODE_320x200
    sw      t1, 0(t0)
    lw      t2, 0(t0)
    bne     t1, t2, fail2

    # Test 3: Set mode back to 640x400x4bpp
    li      t0, DISPLAY_MODE
    li      t1, MODE_640x400
    sw      t1, 0(t0)
    lw      t2, 0(t0)
    bne     t1, t2, fail3

    # Test 4: Write palette entry 0 (RGB565)
    li      t0, DISPLAY_PAL
    li      t1, 0xF800      # Red (RGB565)
    sw      t1, 0(t0)
    lw      t2, 0(t0)
    li      t3, 0xFFFF
    and     t2, t2, t3      # Mask to 16-bit
    bne     t1, t2, fail4

    # Test 5: Write palette entry 1
    li      t0, DISPLAY_PAL + 2
    li      t1, 0x07E0      # Green (RGB565)
    sh      t1, 0(t0)
    lhu     t2, 0(t0)
    bne     t1, t2, fail5

    # Test 6: Write palette entry 15
    li      t0, DISPLAY_PAL + 30  # Entry 15 at offset 30
    li      t1, 0x001F      # Blue (RGB565)
    sh      t1, 0(t0)
    lhu     t2, 0(t0)
    bne     t1, t2, fail6

    # Test 7: Write pixels to framebuffer (4bpp mode)
    # In 4bpp mode, each byte = 2 pixels
    li      t0, FB_BASE
    li      t1, 0x01        # Pixel 0 = color 0, Pixel 1 = color 1
    sb      t1, 0(t0)
    li      t1, 0x23        # Pixel 2 = color 2, Pixel 3 = color 3
    sb      t1, 1(t0)

    # Read back
    lbu     t2, 0(t0)
    li      t3, 0x01
    bne     t2, t3, fail7

    lbu     t2, 1(t0)
    li      t3, 0x23
    bne     t2, t3, fail7

    # Test 8: Switch to 16bpp and write RGB565 pixel
    li      t0, DISPLAY_MODE
    li      t1, MODE_320x200
    sw      t1, 0(t0)

    # Write RGB565 pixels to framebuffer
    li      t0, FB_BASE
    li      t1, 0xFFFF      # White
    sh      t1, 0(t0)
    li      t1, 0x0000      # Black
    sh      t1, 2(t0)

    # Read back
    lhu     t2, 0(t0)
    li      t3, 0xFFFF
    bne     t2, t3, fail8

    lhu     t2, 2(t0)
    bnez    t2, fail8

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

fail8:
    li      gp, 17
    li      a0, 1
    ecall
