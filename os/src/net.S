# COSMO-32 Network Stack
# UDP/TFTP Client Implementation

#include "const.h"
#include "config.h"

.option norvc

.section .data
# Network state
net_initialized:    .word 0
file_size:          .word 0
tftp_block:         .hword 0

.section .text
.global net_init
.global tftp_get
.global tftp_put
.global tftp_dir

#----------------------------------------------------------------------
# net_init: Initialize Ethernet MAC and DMA
# Returns: a0 = 1 if success
#----------------------------------------------------------------------
net_init:
    addi    sp, sp, -4
    sw      ra, 0(sp)

    # Check if already initialized
    la      t0, net_initialized
    lw      t1, 0(t0)
    bnez    t1, net_init_done

    # Set MAC address: 00:02:03:04:05:06
    li      t0, ETH_BASE
    li      t1, 0x0002
    sw      t1, ETH_MACA0HR(t0)
    li      t1, 0x03040506
    sw      t1, ETH_MACA0LR(t0)

    # Setup TX descriptor
    li      t0, TX_DESC
    sw      zero, 0(t0)             # TDES0: not owned yet
    sw      zero, 4(t0)             # TDES1: size = 0
    li      t1, TX_BUF
    sw      t1, 8(t0)               # TDES2: buffer address
    li      t1, TX_DESC
    sw      t1, 12(t0)              # TDES3: chain to self

    # Setup RX descriptor
    li      t0, RX_DESC
    lui     t1, 0x80000             # RDES0: OWN bit
    sw      t1, 0(t0)
    li      t1, 1024 | RDES1_RCH    # RDES1: size | chained
    sw      t1, 4(t0)
    li      t1, RX_BUF
    sw      t1, 8(t0)               # RDES2: buffer address
    li      t1, RX_DESC
    sw      t1, 12(t0)              # RDES3: chain to self

    # Set descriptor list addresses
    li      t0, ETH_BASE
    li      t1, TX_DESC
    sw      t1, ETH_DMATDLAR(t0)
    li      t1, RX_DESC
    sw      t1, ETH_DMARDLAR(t0)

    # Enable MAC TX/RX
    li      t1, MACCR_TE | MACCR_RE
    sw      t1, ETH_MACCR(t0)

    # Start DMA TX/RX
    li      t1, DMAOMR_ST | DMAOMR_SR
    sw      t1, ETH_DMAOMR(t0)

    # Mark initialized
    la      t0, net_initialized
    li      t1, 1
    sw      t1, 0(t0)

net_init_done:
    li      a0, 1
    lw      ra, 0(sp)
    addi    sp, sp, 4
    ret

#----------------------------------------------------------------------
# tftp_get: Download file via TFTP
# a0 = filename pointer (null-terminated)
# Returns: a0 = file size (0 if error), a1 = FILE_BUF address
#----------------------------------------------------------------------
tftp_get:
    addi    sp, sp, -28
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    sw      s1, 8(sp)
    sw      s2, 12(sp)
    sw      s3, 16(sp)
    sw      s4, 20(sp)
    sw      s5, 24(sp)
    mv      s0, a0                  # s0 = filename

    # Initialize network
    call    net_init

    # Build and send RRQ
    mv      a0, s0
    call    build_tftp_rrq
    mv      s1, a0                  # s1 = packet size

    # Send packet
    mv      a0, s1
    call    eth_send

    # Receive data blocks
    li      s2, FILE_BUF            # s2 = current write position
    li      s3, 0                   # s3 = total bytes received
    li      s4, 1                   # s4 = expected block number

tftp_get_loop:
    # Wait for RX
    li      a0, 200000              # timeout cycles
    call    eth_recv
    beqz    a0, tftp_get_fail       # timeout

    # Check TFTP opcode at offset 42
    li      t0, RX_BUF + 42
    lbu     t1, 0(t0)
    lbu     t2, 1(t0)
    # Big-endian: opcode = (t1 << 8) | t2
    slli    t1, t1, 8
    or      t1, t1, t2

    # Check for ERROR (opcode 5)
    li      t2, 5
    beq     t1, t2, tftp_get_fail

    # Check for DATA (opcode 3)
    li      t2, 3
    bne     t1, t2, tftp_get_fail

    # Check block number at offset 44
    li      t0, RX_BUF + 44
    lbu     t1, 0(t0)
    lbu     t2, 1(t0)
    slli    t1, t1, 8
    or      t1, t1, t2
    bne     t1, s4, tftp_get_fail   # Wrong block number

    # Copy data (offset 46 onwards)
    # Frame length from RDES0
    li      t0, RX_DESC
    lw      t1, 0(t0)
    # Re-arm RX descriptor (set OWN bit for next receive)
    lui     t4, 0x80000
    sw      t4, 0(t0)
    # Extract frame length (bits 29:16)
    srli    t1, t1, 16
    slli    t1, t1, 18              # Mask to 14 bits (0x3FFF)
    srli    t1, t1, 18
    # Data length = frame_len - 46
    addi    t1, t1, -46
    blez    t1, tftp_get_send_ack

    # Copy data to file buffer
    li      t0, RX_BUF + 46
    mv      t2, t1                  # t2 = data length
tftp_copy_loop:
    beqz    t2, tftp_get_send_ack
    lbu     t3, 0(t0)
    sb      t3, 0(s2)
    addi    t0, t0, 1
    addi    s2, s2, 1
    addi    t2, t2, -1
    j       tftp_copy_loop

tftp_get_send_ack:
    # t1 still has data length
    mv      s5, t1                  # Save data_len in s5 (callee-saved)
    add     s3, s3, t1              # Update total bytes

    # Send ACK for this block
    mv      a0, s4
    call    build_tftp_ack
    mv      a0, a0
    call    eth_send

    # If data < 512, transfer complete
    li      t0, 512
    blt     s5, t0, tftp_get_done   # Use saved data_len

    # Increment block number, continue
    addi    s4, s4, 1
    j       tftp_get_loop

tftp_get_done:
    # Store file size
    la      t0, file_size
    sw      s3, 0(t0)

    # Return size and buffer address
    mv      a0, s3
    li      a1, FILE_BUF
    j       tftp_get_exit

tftp_get_fail:
    li      a0, 0
    li      a1, 0

tftp_get_exit:
    lw      ra, 0(sp)
    lw      s0, 4(sp)
    lw      s1, 8(sp)
    lw      s2, 12(sp)
    lw      s3, 16(sp)
    lw      s4, 20(sp)
    lw      s5, 24(sp)
    addi    sp, sp, 28
    ret

#----------------------------------------------------------------------
# tftp_put: Upload file via TFTP
# a0 = filename, a1 = data pointer, a2 = size
# Returns: a0 = 1 if success, 0 if error
#----------------------------------------------------------------------
tftp_put:
    addi    sp, sp, -28
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    sw      s1, 8(sp)
    sw      s2, 12(sp)
    sw      s3, 16(sp)
    sw      s4, 20(sp)
    sw      s5, 24(sp)
    mv      s0, a0                  # s0 = filename
    mv      s1, a1                  # s1 = data pointer
    mv      s2, a2                  # s2 = size
    li      s3, 0                   # s3 = bytes sent
    li      s4, 1                   # s4 = block number

    # Initialize network
    call    net_init

    # Build and send WRQ
    mv      a0, s0
    call    build_tftp_wrq
    mv      s5, a0                  # s5 = packet size
    mv      a0, s5
    call    eth_send

    # Wait for ACK 0
    li      a0, 200000
    call    eth_recv
    beqz    a0, tftp_put_fail

    # Re-arm RX descriptor
    li      t0, RX_DESC
    lui     t1, 0x80000
    sw      t1, 0(t0)

    # Check ACK opcode and block 0
    li      t0, RX_BUF + 42
    lbu     t1, 0(t0)
    lbu     t2, 1(t0)
    slli    t1, t1, 8
    or      t1, t1, t2
    li      t2, 4                   # ACK opcode
    bne     t1, t2, tftp_put_fail

tftp_put_loop:
    # Send next data block
    mv      a0, s4                  # block number
    add     a1, s1, s3              # data pointer + offset
    mv      t0, s2
    sub     t0, t0, s3              # remaining
    li      t1, 512
    blt     t0, t1, tftp_put_last
    li      a2, 512
    j       tftp_put_send
tftp_put_last:
    mv      a2, t0                  # last block size

tftp_put_send:
    mv      s5, a2                  # save block size
    call    build_tftp_data
    mv      a0, a0
    call    eth_send

    # Wait for ACK
    li      a0, 200000
    call    eth_recv
    beqz    a0, tftp_put_fail

    # Re-arm RX descriptor
    li      t0, RX_DESC
    lui     t1, 0x80000
    sw      t1, 0(t0)

    # Check ACK for our block
    li      t0, RX_BUF + 42
    lbu     t1, 0(t0)
    lbu     t2, 1(t0)
    slli    t1, t1, 8
    or      t1, t1, t2
    li      t2, 4
    bne     t1, t2, tftp_put_fail

    li      t0, RX_BUF + 44
    lbu     t1, 0(t0)
    lbu     t2, 1(t0)
    slli    t1, t1, 8
    or      t1, t1, t2
    bne     t1, s4, tftp_put_fail

    # Update counters
    add     s3, s3, s5
    addi    s4, s4, 1

    # Check if done
    bge     s3, s2, tftp_put_done

    j       tftp_put_loop

tftp_put_done:
    li      a0, 1
    j       tftp_put_exit

tftp_put_fail:
    li      a0, 0

tftp_put_exit:
    lw      ra, 0(sp)
    lw      s0, 4(sp)
    lw      s1, 8(sp)
    lw      s2, 12(sp)
    lw      s3, 16(sp)
    lw      s4, 20(sp)
    lw      s5, 24(sp)
    addi    sp, sp, 28
    ret

#----------------------------------------------------------------------
# tftp_dir: Get directory listing via TFTP (/.dir)
# Returns: a0 = size of listing, a1 = FILE_BUF address
#----------------------------------------------------------------------
tftp_dir:
    addi    sp, sp, -4
    sw      ra, 0(sp)

    la      a0, str_dir_file
    call    tftp_get

    lw      ra, 0(sp)
    addi    sp, sp, 4
    ret

.section .rodata
str_dir_file:   .asciz "/.dir"

.section .text

#----------------------------------------------------------------------
# eth_send: Transmit packet from TX_BUF
# a0 = packet size
#----------------------------------------------------------------------
eth_send:
    # Setup TX descriptor
    li      t0, TX_DESC

    # TDES0: OWN | FS | LS | TCH
    lui     t1, 0x80000
    li      t2, TDES0_FS | TDES0_LS | TDES0_TCH
    or      t1, t1, t2
    sw      t1, 0(t0)

    # TDES1: size
    sw      a0, 4(t0)

    # Trigger TX poll
    li      t0, ETH_BASE
    sw      zero, ETH_DMATPDR(t0)

    # Wait for TX complete
    li      t1, 100000
eth_send_wait:
    lw      t2, ETH_DMASR(t0)
    andi    t3, t2, DMASR_TS
    bnez    t3, eth_send_done
    addi    t1, t1, -1
    bnez    t1, eth_send_wait

eth_send_done:
    # Clear TX status
    li      t1, DMASR_TS
    sw      t1, ETH_DMASR(t0)
    ret

#----------------------------------------------------------------------
# eth_recv: Wait for received packet
# a0 = timeout (cycles)
# Returns: a0 = 1 if received, 0 if timeout
#----------------------------------------------------------------------
eth_recv:
    mv      t0, a0                  # timeout counter
    li      t1, ETH_BASE

eth_recv_wait:
    lw      t2, ETH_DMASR(t1)
    andi    t3, t2, DMASR_RS
    bnez    t3, eth_recv_got

    addi    t0, t0, -1
    bnez    t0, eth_recv_wait

    # Timeout
    li      a0, 0
    ret

eth_recv_got:
    # Clear RX status
    li      t2, DMASR_RS
    sw      t2, ETH_DMASR(t1)

    # NOTE: Do NOT re-arm RX descriptor here!
    # Caller must read RDES0 for frame length first, then re-arm.

    li      a0, 1
    ret

#----------------------------------------------------------------------
# build_tftp_rrq: Build TFTP Read Request packet
# a0 = filename pointer
# Returns: a0 = packet size
#----------------------------------------------------------------------
build_tftp_rrq:
    addi    sp, sp, -8
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    mv      s0, a0                  # s0 = filename

    li      t0, TX_BUF

    # Ethernet header (14 bytes)
    # Dest MAC: 02:00:00:00:00:01 (server)
    li      t1, 0x00000002
    sw      t1, 0(t0)
    li      t1, 0x0100
    sh      t1, 4(t0)

    # Source MAC: 00:02:03:04:05:06
    li      t1, 0x0302
    sh      t1, 6(t0)
    li      t1, 0x06050403
    sw      t1, 8(t0)

    # EtherType: 0x0800 (IPv4)
    li      t1, 0x0008
    sh      t1, 12(t0)

    # IP header at offset 14
    li      t1, 0x0045              # Version/IHL
    sh      t1, 14(t0)
    # Total length - fill later
    sh      zero, 16(t0)
    sw      zero, 18(t0)            # ID, flags, fragment
    li      t1, 0x1140              # TTL=64, Proto=UDP
    sh      t1, 22(t0)
    sh      zero, 24(t0)            # Checksum - fill later
    li      t1, OUR_IP
    sw      t1, 26(t0)
    li      t1, SERVER_IP
    sw      t1, 30(t0)

    # UDP header at offset 34
    li      t1, CLIENT_PORT
    # Store big-endian
    andi    t2, t1, 0xFF
    slli    t2, t2, 8
    srli    t3, t1, 8
    or      t1, t2, t3
    sh      t1, 34(t0)

    li      t1, TFTP_PORT
    andi    t2, t1, 0xFF
    slli    t2, t2, 8
    srli    t3, t1, 8
    or      t1, t2, t3
    sh      t1, 36(t0)
    # UDP length - fill later
    sh      zero, 38(t0)
    sh      zero, 40(t0)            # UDP checksum (optional)

    # TFTP RRQ at offset 42
    li      t1, 0x0100              # Opcode 1 (big-endian)
    sh      t1, 42(t0)

    # Copy filename at offset 44
    addi    t1, t0, 44
    mv      t2, s0
rrq_copy_fname:
    lbu     t3, 0(t2)
    sb      t3, 0(t1)
    addi    t1, t1, 1
    addi    t2, t2, 1
    bnez    t3, rrq_copy_fname

    # Add mode "octet"
    li      t3, 'o'
    sb      t3, 0(t1)
    li      t3, 'c'
    sb      t3, 1(t1)
    li      t3, 't'
    sb      t3, 2(t1)
    li      t3, 'e'
    sb      t3, 3(t1)
    li      t3, 't'
    sb      t3, 4(t1)
    sb      zero, 5(t1)
    addi    t1, t1, 6

    # Calculate packet size
    li      t0, TX_BUF
    sub     a0, t1, t0              # a0 = total size

    # Fill in IP total length
    addi    t2, a0, -14             # IP length = total - 14
    andi    t3, t2, 0xFF
    slli    t3, t3, 8
    srli    t4, t2, 8
    or      t2, t3, t4
    sh      t2, 16(t0)

    # Fill in UDP length
    addi    t2, a0, -34             # UDP length = total - 34
    andi    t3, t2, 0xFF
    slli    t3, t3, 8
    srli    t4, t2, 8
    or      t2, t3, t4
    sh      t2, 38(t0)

    # Calculate IP checksum
    addi    a1, t0, 14
    call    calc_ip_checksum

    lw      ra, 0(sp)
    lw      s0, 4(sp)
    addi    sp, sp, 8
    ret

#----------------------------------------------------------------------
# build_tftp_wrq: Build TFTP Write Request packet
# a0 = filename pointer
# Returns: a0 = packet size
#----------------------------------------------------------------------
build_tftp_wrq:
    addi    sp, sp, -8
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    mv      s0, a0

    li      t0, TX_BUF

    # Ethernet + IP + UDP header (same as RRQ)
    li      t1, 0x00000002
    sw      t1, 0(t0)
    li      t1, 0x0100
    sh      t1, 4(t0)
    li      t1, 0x0302
    sh      t1, 6(t0)
    li      t1, 0x06050403
    sw      t1, 8(t0)
    li      t1, 0x0008
    sh      t1, 12(t0)
    li      t1, 0x0045
    sh      t1, 14(t0)
    sh      zero, 16(t0)
    sw      zero, 18(t0)
    li      t1, 0x1140
    sh      t1, 22(t0)
    sh      zero, 24(t0)
    li      t1, OUR_IP
    sw      t1, 26(t0)
    li      t1, SERVER_IP
    sw      t1, 30(t0)

    li      t1, CLIENT_PORT
    andi    t2, t1, 0xFF
    slli    t2, t2, 8
    srli    t3, t1, 8
    or      t1, t2, t3
    sh      t1, 34(t0)
    li      t1, TFTP_PORT
    andi    t2, t1, 0xFF
    slli    t2, t2, 8
    srli    t3, t1, 8
    or      t1, t2, t3
    sh      t1, 36(t0)
    sh      zero, 38(t0)
    sh      zero, 40(t0)

    # TFTP WRQ: opcode 2
    li      t1, 0x0200
    sh      t1, 42(t0)

    # Copy filename
    addi    t1, t0, 44
    mv      t2, s0
wrq_copy_fname:
    lbu     t3, 0(t2)
    sb      t3, 0(t1)
    addi    t1, t1, 1
    addi    t2, t2, 1
    bnez    t3, wrq_copy_fname

    # Add mode "octet"
    li      t3, 'o'
    sb      t3, 0(t1)
    li      t3, 'c'
    sb      t3, 1(t1)
    li      t3, 't'
    sb      t3, 2(t1)
    li      t3, 'e'
    sb      t3, 3(t1)
    li      t3, 't'
    sb      t3, 4(t1)
    sb      zero, 5(t1)
    addi    t1, t1, 6

    li      t0, TX_BUF
    sub     a0, t1, t0

    addi    t2, a0, -14
    andi    t3, t2, 0xFF
    slli    t3, t3, 8
    srli    t4, t2, 8
    or      t2, t3, t4
    sh      t2, 16(t0)

    addi    t2, a0, -34
    andi    t3, t2, 0xFF
    slli    t3, t3, 8
    srli    t4, t2, 8
    or      t2, t3, t4
    sh      t2, 38(t0)

    addi    a1, t0, 14
    call    calc_ip_checksum

    lw      ra, 0(sp)
    lw      s0, 4(sp)
    addi    sp, sp, 8
    ret

#----------------------------------------------------------------------
# build_tftp_ack: Build TFTP ACK packet
# a0 = block number
# Returns: a0 = packet size (46)
#----------------------------------------------------------------------
build_tftp_ack:
    mv      t5, a0                  # t5 = block number

    li      t0, TX_BUF

    # Ethernet header
    li      t1, 0x00000002
    sw      t1, 0(t0)
    li      t1, 0x0100
    sh      t1, 4(t0)
    li      t1, 0x0302
    sh      t1, 6(t0)
    li      t1, 0x06050403
    sw      t1, 8(t0)
    li      t1, 0x0008
    sh      t1, 12(t0)

    # IP header
    li      t1, 0x0045
    sh      t1, 14(t0)
    li      t1, 0x2000              # Total length = 32 (20 + 8 + 4) big-endian
    sh      t1, 16(t0)
    sw      zero, 18(t0)
    li      t1, 0x1140
    sh      t1, 22(t0)
    sh      zero, 24(t0)
    li      t1, OUR_IP
    sw      t1, 26(t0)
    li      t1, SERVER_IP
    sw      t1, 30(t0)

    # UDP header
    li      t1, CLIENT_PORT
    andi    t2, t1, 0xFF
    slli    t2, t2, 8
    srli    t3, t1, 8
    or      t1, t2, t3
    sh      t1, 34(t0)
    li      t1, TFTP_PORT
    andi    t2, t1, 0xFF
    slli    t2, t2, 8
    srli    t3, t1, 8
    or      t1, t2, t3
    sh      t1, 36(t0)
    li      t1, 0x0C00              # UDP length = 12 (8 + 4) big-endian
    sh      t1, 38(t0)
    sh      zero, 40(t0)

    # TFTP ACK
    li      t1, 0x0400              # Opcode 4 (big-endian)
    sh      t1, 42(t0)
    # Block number (big-endian)
    andi    t1, t5, 0xFF
    slli    t1, t1, 8
    srli    t2, t5, 8
    or      t1, t1, t2
    sh      t1, 44(t0)

    # Calculate IP checksum
    addi    sp, sp, -4
    sw      ra, 0(sp)
    addi    a1, t0, 14
    li      a0, 46
    call    calc_ip_checksum
    lw      ra, 0(sp)
    addi    sp, sp, 4

    li      a0, 46
    ret

#----------------------------------------------------------------------
# build_tftp_data: Build TFTP DATA packet
# a0 = block number, a1 = data pointer, a2 = data size
# Returns: a0 = packet size
#----------------------------------------------------------------------
build_tftp_data:
    addi    sp, sp, -16
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    sw      s1, 8(sp)
    sw      s2, 12(sp)
    mv      s0, a0                  # block number
    mv      s1, a1                  # data pointer
    mv      s2, a2                  # data size

    li      t0, TX_BUF

    # Ethernet header
    li      t1, 0x00000002
    sw      t1, 0(t0)
    li      t1, 0x0100
    sh      t1, 4(t0)
    li      t1, 0x0302
    sh      t1, 6(t0)
    li      t1, 0x06050403
    sw      t1, 8(t0)
    li      t1, 0x0008
    sh      t1, 12(t0)

    # IP header (length filled later)
    li      t1, 0x0045
    sh      t1, 14(t0)
    sh      zero, 16(t0)
    sw      zero, 18(t0)
    li      t1, 0x1140
    sh      t1, 22(t0)
    sh      zero, 24(t0)
    li      t1, OUR_IP
    sw      t1, 26(t0)
    li      t1, SERVER_IP
    sw      t1, 30(t0)

    # UDP header
    li      t1, CLIENT_PORT
    andi    t2, t1, 0xFF
    slli    t2, t2, 8
    srli    t3, t1, 8
    or      t1, t2, t3
    sh      t1, 34(t0)
    li      t1, TFTP_PORT
    andi    t2, t1, 0xFF
    slli    t2, t2, 8
    srli    t3, t1, 8
    or      t1, t2, t3
    sh      t1, 36(t0)
    sh      zero, 38(t0)
    sh      zero, 40(t0)

    # TFTP DATA
    li      t1, 0x0300              # Opcode 3 (big-endian)
    sh      t1, 42(t0)
    # Block number (big-endian)
    andi    t1, s0, 0xFF
    slli    t1, t1, 8
    srli    t2, s0, 8
    or      t1, t1, t2
    sh      t1, 44(t0)

    # Copy data at offset 46
    addi    t0, t0, 46
    mv      t1, s1
    mv      t2, s2
data_copy_loop:
    beqz    t2, data_copy_done
    lbu     t3, 0(t1)
    sb      t3, 0(t0)
    addi    t0, t0, 1
    addi    t1, t1, 1
    addi    t2, t2, -1
    j       data_copy_loop

data_copy_done:
    # Calculate total size
    li      t0, TX_BUF
    addi    a0, s2, 46              # 46 + data size

    # Fill IP total length
    addi    t2, a0, -14
    andi    t3, t2, 0xFF
    slli    t3, t3, 8
    srli    t4, t2, 8
    or      t2, t3, t4
    sh      t2, 16(t0)

    # Fill UDP length
    addi    t2, a0, -34
    andi    t3, t2, 0xFF
    slli    t3, t3, 8
    srli    t4, t2, 8
    or      t2, t3, t4
    sh      t2, 38(t0)

    # Calculate IP checksum
    addi    a1, t0, 14
    call    calc_ip_checksum

    addi    a0, s2, 46

    lw      ra, 0(sp)
    lw      s0, 4(sp)
    lw      s1, 8(sp)
    lw      s2, 12(sp)
    addi    sp, sp, 16
    ret

#----------------------------------------------------------------------
# calc_ip_checksum: Calculate and store IP header checksum
# a0 = packet size (for return), a1 = IP header pointer
#----------------------------------------------------------------------
calc_ip_checksum:
    addi    sp, sp, -4
    sw      s0, 0(sp)
    mv      s0, a0                  # save packet size

    mv      t0, a1                  # IP header start
    li      t1, 0                   # sum
    li      t2, 10                  # 10 halfwords

    # Clear checksum field
    sh      zero, 10(t0)

checksum_loop:
    lhu     t3, 0(t0)
    # Swap bytes for big-endian sum
    andi    t4, t3, 0xFF
    slli    t4, t4, 8
    srli    t5, t3, 8
    or      t3, t4, t5
    add     t1, t1, t3
    addi    t0, t0, 2
    addi    t2, t2, -1
    bnez    t2, checksum_loop

    # Fold 32-bit to 16-bit
    srli    t2, t1, 16
    slli    t1, t1, 16              # Mask to 16 bits
    srli    t1, t1, 16
    add     t1, t1, t2
    srli    t2, t1, 16
    add     t1, t1, t2
    slli    t1, t1, 16
    srli    t1, t1, 16

    # One's complement
    not     t1, t1
    slli    t1, t1, 16
    srli    t1, t1, 16

    # Swap back for storage
    andi    t2, t1, 0xFF
    slli    t2, t2, 8
    srli    t3, t1, 8
    or      t1, t2, t3

    # Store checksum
    sh      t1, 10(a1)

    mv      a0, s0                  # restore packet size
    lw      s0, 0(sp)
    addi    sp, sp, 4
    ret
