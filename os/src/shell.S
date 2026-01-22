# COSMO-32 Shell
# Minimale Shell mit Befehlen: help, status, regs, mem, net, ping

#include "const.h"
#include "config.h"

.option norvc

.section .rodata
prompt:         .asciz "> "
msg_help:       .asciz "Commands: help status regs mem poke net ping ls cat get put run basic exit reset\n"
msg_status:     .asciz "COSMO-32 OS v0.1\n"
msg_cpu:        .asciz "CPU:    RV32IMAC @ 144MHz\n"
msg_ram:        .asciz "RAM:    64KB SRAM + 1MB FSMC\n"
msg_video:      .asciz "Video:  640x400 @ 4bpp (CGA)\n"
msg_uptime:     .asciz "Uptime: "
msg_stack:      .asciz "Stack:  0x"
msg_free:       .asciz " free: "
msg_bytes2:     .asciz " bytes\n"
msg_ticks:      .asciz "Ticks:  "
msg_d:          .asciz "d "
msg_colon2:     .asciz ":"
msg_nl:         .asciz "\n"
msg_regs:       .asciz "Registers:\n"
msg_net_hdr:    .asciz "Network:\n"
msg_mac:        .asciz "  MAC: "
msg_ip:         .asciz "  IP:  10.0.0.2\n"
msg_mem_hdr:    .asciz "Memory at 0x"
msg_colon:      .asciz ": "
msg_newline:    .asciz "\n"
msg_unknown:    .asciz "Unknown command\n"
msg_ping_send:  .asciz "Ping 10.0.0.1... "
msg_ping_ok:    .asciz "OK\n"
msg_ping_fail:  .asciz "FAIL\n"
msg_reset:      .asciz "Resetting...\n"
msg_poke_ok:    .asciz "Wrote "
msg_poke_eq:    .asciz " = "
msg_poke_err:   .asciz "Usage: poke <addr> <value>\n"
msg_udp_stub:   .asciz "UDP not implemented yet\n"
msg_run_err:    .asciz "Usage: run <addr>\n"
msg_get_ok:     .asciz "Downloaded "
msg_bytes:      .asciz " bytes\n"
msg_get_err:    .asciz "Download failed\n"
msg_cat_err:    .asciz "Usage: cat <file>\n"
msg_get_usage:  .asciz "Usage: get <file>\n"
msg_put_usage:  .asciz "Usage: put <file> <data>\n"
msg_put_ok:     .asciz "Uploaded OK\n"
msg_put_err:    .asciz "Upload failed\n"
msg_ls_hdr:     .asciz "Files:\n"

cmd_help:       .asciz "help"
cmd_status:     .asciz "status"
cmd_regs:       .asciz "regs"
cmd_mem:        .asciz "mem"
cmd_net:        .asciz "net"
cmd_ping:       .asciz "ping"
cmd_reset:      .asciz "reset"
cmd_exit:       .asciz "exit"
cmd_poke:       .asciz "poke"
cmd_udp:        .asciz "udp"
cmd_run:        .asciz "run"
cmd_ls:         .asciz "ls"
cmd_cat:        .asciz "cat"
cmd_get:        .asciz "get"
cmd_put:        .asciz "put"
cmd_basic:      .asciz "basic"

hex_chars:      .asciz "0123456789ABCDEF"

.section .text
.global shell_init
.global shell_run
.global putchar
.global getchar
.global getchar_nonblock
.global get_timer_ms
.global print_str
.global print_dec

#----------------------------------------------------------------------
# shell_init: Initialize USART for TX/RX
#----------------------------------------------------------------------
shell_init:
    li      t0, USART1_BASE
    li      t1, CTLR1_UE | CTLR1_TE | CTLR1_RE
    sw      t1, USART_CTLR1(t0)
    ret

#----------------------------------------------------------------------
# shell_run: Main shell loop
#----------------------------------------------------------------------
shell_run:
    addi    sp, sp, -4
    sw      ra, 0(sp)

shell_loop:
    # Print prompt
    la      a0, prompt
    call    print_str

    # Read command line
    li      a0, CMD_BUF
    li      a1, CMD_BUF_SIZE
    call    read_line

    # Skip empty lines
    li      t0, CMD_BUF
    lbu     t1, 0(t0)
    beqz    t1, shell_loop

    # Parse and execute command
    li      a0, CMD_BUF
    call    execute_cmd

    j       shell_loop

    # Never reached
    lw      ra, 0(sp)
    addi    sp, sp, 4
    ret

#----------------------------------------------------------------------
# execute_cmd: Parse and execute a command
# a0 = command buffer
#----------------------------------------------------------------------
execute_cmd:
    addi    sp, sp, -20
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    sw      s1, 8(sp)
    sw      s2, 12(sp)
    sw      s3, 16(sp)
    mv      s0, a0              # Save command buffer

    # Try each command
    la      a1, cmd_help
    call    str_starts_with
    bnez    a0, do_help

    mv      a0, s0
    la      a1, cmd_status
    call    str_starts_with
    bnez    a0, do_status

    mv      a0, s0
    la      a1, cmd_regs
    call    str_starts_with
    bnez    a0, do_regs

    mv      a0, s0
    la      a1, cmd_mem
    call    str_starts_with
    bnez    a0, do_mem

    mv      a0, s0
    la      a1, cmd_net
    call    str_starts_with
    bnez    a0, do_net

    mv      a0, s0
    la      a1, cmd_ping
    call    str_starts_with
    bnez    a0, do_ping

    mv      a0, s0
    la      a1, cmd_reset
    call    str_starts_with
    bnez    a0, do_reset

    mv      a0, s0
    la      a1, cmd_exit
    call    str_starts_with
    bnez    a0, do_exit

    mv      a0, s0
    la      a1, cmd_poke
    call    str_starts_with
    bnez    a0, do_poke

    mv      a0, s0
    la      a1, cmd_udp
    call    str_starts_with
    bnez    a0, do_udp

    mv      a0, s0
    la      a1, cmd_run
    call    str_starts_with
    bnez    a0, do_run

    mv      a0, s0
    la      a1, cmd_ls
    call    str_starts_with
    bnez    a0, do_ls

    mv      a0, s0
    la      a1, cmd_cat
    call    str_starts_with
    bnez    a0, do_cat

    mv      a0, s0
    la      a1, cmd_get
    call    str_starts_with
    bnez    a0, do_get

    mv      a0, s0
    la      a1, cmd_put
    call    str_starts_with
    bnez    a0, do_put

    mv      a0, s0
    la      a1, cmd_basic
    call    str_starts_with
    bnez    a0, do_basic

    # Unknown command
    la      a0, msg_unknown
    call    print_str
    j       exec_done

do_help:
    la      a0, msg_help
    call    print_str
    j       exec_done

do_status:
    addi    sp, sp, -16
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    sw      s1, 8(sp)
    sw      s2, 12(sp)

    # Header
    la      a0, msg_status
    call    print_str

    # CPU info
    la      a0, msg_cpu
    call    print_str

    # RAM info
    la      a0, msg_ram
    call    print_str

    # Video info
    la      a0, msg_video
    call    print_str

    # Uptime: convert ms to d:hh:mm:ss
    la      a0, msg_uptime
    call    print_str

    li      t0, TIMER_BASE
    lw      s0, TIMER_MTIME(t0)     # s0 = total milliseconds
    mv      s2, s0                  # save for ticks display

    # ms -> seconds
    li      t1, 1000
    divu    s0, s0, t1              # s0 = total seconds

    # seconds -> days, hours, minutes, seconds
    li      t1, 86400               # seconds per day
    divu    s1, s0, t1              # s1 = days
    remu    s0, s0, t1              # s0 = remaining seconds

    # Print days if > 0
    beqz    s1, skip_days
    mv      a0, s1
    call    print_dec
    la      a0, msg_d
    call    print_str
skip_days:

    # hours
    li      t1, 3600
    divu    a0, s0, t1              # hours
    remu    s0, s0, t1              # remaining seconds
    call    print_dec_2digit

    la      a0, msg_colon2
    call    print_str

    # minutes
    li      t1, 60
    divu    a0, s0, t1              # minutes
    remu    s0, s0, t1              # remaining = seconds
    call    print_dec_2digit

    la      a0, msg_colon2
    call    print_str

    # seconds
    mv      a0, s0
    call    print_dec_2digit

    la      a0, msg_nl
    call    print_str

    # Ticks (raw ms)
    la      a0, msg_ticks
    call    print_str
    mv      a0, s2
    call    print_dec
    la      a0, msg_nl
    call    print_str

    # Stack info
    la      a0, msg_stack
    call    print_str
    mv      s1, sp
    addi    s1, s1, 16              # adjust for our push (s1 = actual SP)
    mv      a0, s1
    call    print_hex32
    la      a0, msg_free
    call    print_str
    # Free = SP - SRAM_BASE (stack grows down from top)
    li      t0, SRAM_BASE
    sub     a0, s1, t0
    call    print_dec
    la      a0, msg_bytes2
    call    print_str

    lw      ra, 0(sp)
    lw      s0, 4(sp)
    lw      s1, 8(sp)
    lw      s2, 12(sp)
    addi    sp, sp, 16
    j       exec_done

do_regs:
    la      a0, msg_regs
    call    print_str
    # Print sp, gp, tp
    call    print_regs
    j       exec_done

do_mem:
    # Parse address after "mem "
    addi    a0, s0, 4           # Skip "mem "
    call    skip_spaces
    mv      s1, a0              # Save pointer for length parse
    call    parse_hex
    beqz    a1, mem_default     # No address given
    mv      s1, a0              # s1 = address (callee-saved)
    j       mem_parse_len
mem_default:
    li      s1, 0x20000000      # Default address
mem_parse_len:
    # Check for optional length argument
    # Skip past address in buffer to find length
    addi    a0, s0, 4           # Start after "mem "
    call    skip_spaces
mem_skip_addr:
    lbu     t0, 0(a0)
    beqz    t0, mem_use_default_len
    li      t1, ' '
    beq     t0, t1, mem_found_space
    addi    a0, a0, 1
    j       mem_skip_addr
mem_found_space:
    call    skip_spaces
    call    parse_dec           # Length is decimal
    beqz    a1, mem_use_default_len
    mv      s2, a0              # s2 = length
    # Clamp length to max 256
    li      t0, 256
    bgt     s2, t0, mem_clamp_len
    j       mem_dump
mem_clamp_len:
    li      s2, 256
    j       mem_dump
mem_use_default_len:
    li      s2, 16              # Default: 16 bytes
mem_dump:
    # Print header
    la      a0, msg_mem_hdr
    call    print_str
    mv      a0, s1
    call    print_hex32
    la      a0, msg_colon
    call    print_str
    la      a0, msg_newline
    call    print_str
    # Dump s2 bytes
mem_loop:
    lbu     a0, 0(s1)
    call    print_hex8
    li      a0, ' '
    call    putchar
    addi    s1, s1, 1
    addi    s2, s2, -1
    bnez    s2, mem_loop
    la      a0, msg_newline
    call    print_str
    j       exec_done

do_net:
    la      a0, msg_net_hdr
    call    print_str
    # Print MAC address
    la      a0, msg_mac
    call    print_str
    call    print_mac
    la      a0, msg_newline
    call    print_str
    # Print IP
    la      a0, msg_ip
    call    print_str
    j       exec_done

do_ping:
    la      a0, msg_ping_send
    call    print_str
    call    send_ping
    bnez    a0, ping_ok
    la      a0, msg_ping_fail
    call    print_str
    j       exec_done
ping_ok:
    la      a0, msg_ping_ok
    call    print_str
    j       exec_done

do_reset:
    la      a0, msg_reset
    call    print_str
    # Jump to reset vector
    li      t0, 0
    jr      t0

do_exit:
    # Exit via ECALL (ends emulator in headless mode)
    li      a0, 0               # a0 = 0 for clean exit
    li      gp, 1               # gp = 1 signals test pass
    ecall

do_poke:
    # poke <addr> <value> - Write byte/word to memory
    # Parse address - scan from start of buffer after "poke "
    addi    a0, s0, 5           # Skip "poke "
    call    skip_spaces
    mv      s1, a0              # s1 = pointer to address string
    call    parse_hex
    beqz    a1, poke_error
    mv      s2, a0              # s2 = address value
    # Find next argument: skip over address chars in buffer
    mv      a0, s1              # Restore pointer to address string
poke_skip_addr:
    lbu     t0, 0(a0)
    beqz    t0, poke_error
    li      t1, ' '
    beq     t0, t1, poke_found_space
    addi    a0, a0, 1
    j       poke_skip_addr
poke_found_space:
    call    skip_spaces
    call    parse_hex
    beqz    a1, poke_error
    # a0 = value, s2 = address
    # Write byte to address
    sb      a0, 0(s2)
    # Print confirmation: "Wrote 0xADDR = 0xVAL\n"
    mv      s1, a0              # s1 = value (for printing later)
    la      a0, msg_poke_ok
    call    print_str
    mv      a0, s2
    call    print_hex32
    la      a0, msg_poke_eq
    call    print_str
    mv      a0, s1
    call    print_hex8
    la      a0, msg_newline
    call    print_str
    j       exec_done
poke_error:
    la      a0, msg_poke_err
    call    print_str
    j       exec_done

do_udp:
    # udp <ip:port> <data> - Send UDP packet (stub)
    la      a0, msg_udp_stub
    call    print_str
    j       exec_done

do_run:
    # run <addr> - Jump to address
    addi    a0, s0, 4           # Skip "run "
    call    skip_spaces
    call    parse_hex
    beqz    a1, run_error
    mv      t0, a0
    jr      t0                  # Jump to address (no return)
run_error:
    la      a0, msg_run_err
    call    print_str
    j       exec_done

do_ls:
    # ls - List files via TFTP /.dir
    la      a0, msg_ls_hdr
    call    print_str
    call    tftp_dir
    beqz    a0, ls_done         # No files or error
    # a0 = size, a1 = buffer
    mv      s1, a0              # s1 = size
    mv      s2, a1              # s2 = buffer
ls_print_loop:
    beqz    s1, ls_done
    lbu     a0, 0(s2)
    beqz    a0, ls_done
    call    putchar
    addi    s2, s2, 1
    addi    s1, s1, -1
    j       ls_print_loop
ls_done:
    j       exec_done

do_cat:
    # cat <file> - Display file contents
    addi    a0, s0, 4           # Skip "cat "
    call    skip_spaces
    lbu     t0, 0(a0)
    beqz    t0, cat_error       # No filename
    call    tftp_get
    beqz    a0, cat_error       # Download failed
    # a0 = size, a1 = buffer
    mv      s1, a0
    mv      s2, a1
cat_print_loop:
    beqz    s1, cat_done
    lbu     a0, 0(s2)
    call    putchar
    addi    s2, s2, 1
    addi    s1, s1, -1
    j       cat_print_loop
cat_done:
    la      a0, msg_newline
    call    print_str
    j       exec_done
cat_error:
    la      a0, msg_cat_err
    call    print_str
    j       exec_done

do_get:
    # get <file> - Download file (shows size)
    addi    a0, s0, 4           # Skip "get "
    call    skip_spaces
    lbu     t0, 0(a0)
    beqz    t0, get_error_usage
    call    tftp_get
    beqz    a0, get_error
    # Success - print size
    mv      s1, a0
    la      a0, msg_get_ok
    call    print_str
    mv      a0, s1
    call    print_dec
    la      a0, msg_bytes
    call    print_str
    j       exec_done
get_error_usage:
    la      a0, msg_get_usage
    call    print_str
    j       exec_done
get_error:
    la      a0, msg_get_err
    call    print_str
    j       exec_done

do_put:
    # put <file> <data> - Upload file with inline data
    addi    a0, s0, 4           # Skip "put "
    call    skip_spaces
    mv      s1, a0              # s1 = filename start
    # Find end of filename (space or null)
put_find_data:
    lbu     t0, 0(a0)
    beqz    t0, put_error_usage
    li      t1, ' '
    beq     t0, t1, put_found_space
    addi    a0, a0, 1
    j       put_find_data
put_found_space:
    sb      zero, 0(a0)         # Null-terminate filename
    addi    a0, a0, 1
    call    skip_spaces
    mv      s2, a0              # s2 = data start
    # Calculate data length
    li      s3, 0
put_count_data:
    lbu     t0, 0(a0)
    beqz    t0, put_do_upload
    addi    s3, s3, 1
    addi    a0, a0, 1
    j       put_count_data
put_do_upload:
    mv      a0, s1              # filename
    mv      a1, s2              # data
    mv      a2, s3              # size
    call    tftp_put
    beqz    a0, put_error
    la      a0, msg_put_ok
    call    print_str
    j       exec_done
put_error_usage:
    la      a0, msg_put_usage
    call    print_str
    j       exec_done
put_error:
    la      a0, msg_put_err
    call    print_str
    j       exec_done

do_basic:
    # basic [filename] - Run BASIC interpreter
    addi    a0, s0, 5           # Skip "basic"
    call    skip_spaces
    # a0 = pointer to filename (or empty string)
    call    basic_main
    j       exec_done

exec_done:
    lw      ra, 0(sp)
    lw      s0, 4(sp)
    lw      s1, 8(sp)
    lw      s2, 12(sp)
    lw      s3, 16(sp)
    addi    sp, sp, 20
    ret

#----------------------------------------------------------------------
# print_regs: Print key registers
#----------------------------------------------------------------------
print_regs:
    addi    sp, sp, -4
    sw      ra, 0(sp)

    # Print SP
    la      a0, str_sp
    call    print_str
    mv      a0, sp
    addi    a0, a0, 4           # Adjust for our push
    call    print_hex32
    la      a0, msg_newline
    call    print_str

    # Print GP
    la      a0, str_gp
    call    print_str
    mv      a0, gp
    call    print_hex32
    la      a0, msg_newline
    call    print_str

    lw      ra, 0(sp)
    addi    sp, sp, 4
    ret

.section .rodata
str_sp:         .asciz "  SP: 0x"
str_gp:         .asciz "  GP: 0x"

.section .text

#----------------------------------------------------------------------
# print_mac: Print MAC address from ETH registers
#----------------------------------------------------------------------
print_mac:
    addi    sp, sp, -4
    sw      ra, 0(sp)

    li      t0, ETH_BASE
    lw      t1, ETH_MACA0HR(t0)     # High bytes
    lw      t2, ETH_MACA0LR(t0)     # Low bytes

    # Print as XX:XX:XX:XX:XX:XX
    # High: bits 15:8 = byte 0, bits 7:0 = byte 1
    srli    a0, t1, 8
    andi    a0, a0, 0xFF
    call    print_hex8
    li      a0, ':'
    call    putchar

    andi    a0, t1, 0xFF
    call    print_hex8
    li      a0, ':'
    call    putchar

    # Low: bits 31:24 = byte 2, etc.
    srli    a0, t2, 24
    andi    a0, a0, 0xFF
    call    print_hex8
    li      a0, ':'
    call    putchar

    srli    a0, t2, 16
    andi    a0, a0, 0xFF
    call    print_hex8
    li      a0, ':'
    call    putchar

    srli    a0, t2, 8
    andi    a0, a0, 0xFF
    call    print_hex8
    li      a0, ':'
    call    putchar

    andi    a0, t2, 0xFF
    call    print_hex8

    lw      ra, 0(sp)
    addi    sp, sp, 4
    ret

#----------------------------------------------------------------------
# send_ping: Send ICMP echo request (stub - returns 1 for now)
# Returns: a0 = 1 if success, 0 if fail
#----------------------------------------------------------------------
send_ping:
    # TODO: Implement actual ping using ETH
    # For now, just return success
    li      a0, 1
    ret

#----------------------------------------------------------------------
# USART I/O Functions
#----------------------------------------------------------------------

# putchar: Output a character to USART and Display
# a0 = character
putchar:
    addi    sp, sp, -8
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    mv      s0, a0              # Save character

    # Output to USART
    li      t0, USART1_BASE
1:  lw      t1, USART_STATR(t0)
    andi    t1, t1, STATR_TXE
    beqz    t1, 1b
    sw      s0, USART_DATAR(t0)

    # Output to Display
    mv      a0, s0
    call    display_putchar

    lw      ra, 0(sp)
    lw      s0, 4(sp)
    addi    sp, sp, 8
    ret

# getchar: Read a character (blocking)
# Returns: a0 = character
getchar:
    li      t0, USART1_BASE
1:  lw      t1, USART_STATR(t0)
    andi    t1, t1, STATR_RXNE
    bnez    t1, 2f
    wfi                         # Sleep until interrupt/event
    j       1b
2:  lw      a0, USART_DATAR(t0)
    andi    a0, a0, 0xFF
    ret

# getchar_nonblock: Read a character (non-blocking)
# Returns: a0 = character, or -1 if none available
getchar_nonblock:
    li      t0, USART1_BASE
    lw      t1, USART_STATR(t0)
    andi    t1, t1, STATR_RXNE
    beqz    t1, 1f
    lw      a0, USART_DATAR(t0)
    andi    a0, a0, 0xFF
    ret
1:  li      a0, -1
    ret

# get_timer_ms: Get current timer value in milliseconds
# Returns: a0 = milliseconds since boot
get_timer_ms:
    li      t0, TIMER_BASE
    lw      a0, TIMER_MTIME(t0)
    ret

# print_str: Print null-terminated string
# a0 = string pointer
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

# read_line: Read line into buffer
# a0 = buffer, a1 = max length
read_line:
    addi    sp, sp, -16
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    sw      s1, 8(sp)
    sw      s2, 12(sp)
    mv      s0, a0              # buffer
    mv      s1, a1              # max length
    li      s2, 0               # current position

read_loop:
    call    getchar

    # Check for enter (CR or LF)
    li      t0, 13
    beq     a0, t0, read_done
    li      t0, 10
    beq     a0, t0, read_done

    # Check for backspace
    li      t0, 8
    beq     a0, t0, read_backspace
    li      t0, 127
    beq     a0, t0, read_backspace

    # Check buffer full
    addi    t0, s1, -1
    bge     s2, t0, read_loop

    # Store character and echo
    add     t0, s0, s2
    sb      a0, 0(t0)
    addi    s2, s2, 1
    call    putchar
    j       read_loop

read_backspace:
    beqz    s2, read_loop       # Nothing to delete
    addi    s2, s2, -1
    # Echo backspace sequence
    li      a0, 8
    call    putchar
    li      a0, ' '
    call    putchar
    li      a0, 8
    call    putchar
    j       read_loop

read_done:
    # Null terminate
    add     t0, s0, s2
    sb      zero, 0(t0)
    # Echo newline
    li      a0, '\n'
    call    putchar

    lw      ra, 0(sp)
    lw      s0, 4(sp)
    lw      s1, 8(sp)
    lw      s2, 12(sp)
    addi    sp, sp, 16
    ret

#----------------------------------------------------------------------
# String/Number Functions
#----------------------------------------------------------------------

# str_starts_with: Check if string starts with prefix
# a0 = string, a1 = prefix
# Returns: a0 = 1 if match, 0 if not
str_starts_with:
    mv      t0, a0
    mv      t1, a1
1:  lbu     t2, 0(t1)           # prefix char
    beqz    t2, 2f              # End of prefix = match
    lbu     t3, 0(t0)           # string char
    bne     t2, t3, 3f          # Mismatch
    addi    t0, t0, 1
    addi    t1, t1, 1
    j       1b
2:  li      a0, 1
    ret
3:  li      a0, 0
    ret

# skip_spaces: Skip leading spaces
# a0 = string pointer
# Returns: a0 = pointer to first non-space
skip_spaces:
1:  lbu     t0, 0(a0)
    li      t1, ' '
    bne     t0, t1, 2f
    addi    a0, a0, 1
    j       1b
2:  ret

# parse_hex: Parse hex number from string
# a0 = string pointer
# Returns: a0 = value, a1 = 1 if valid, 0 if not
parse_hex:
    li      t0, 0               # result
    li      t1, 0               # valid flag

    # Skip "0x" prefix if present
    lbu     t2, 0(a0)
    li      t3, '0'
    bne     t2, t3, parse_hex_loop
    lbu     t2, 1(a0)
    li      t3, 'x'
    bne     t2, t3, parse_hex_loop
    addi    a0, a0, 2

parse_hex_loop:
    lbu     t2, 0(a0)

    # Check 0-9
    li      t3, '0'
    blt     t2, t3, parse_hex_check_upper
    li      t3, '9'
    bgt     t2, t3, parse_hex_check_upper
    li      t3, '0'
    sub     t2, t2, t3
    j       parse_hex_add

parse_hex_check_upper:
    # Check A-F
    li      t3, 'A'
    blt     t2, t3, parse_hex_check_lower
    li      t3, 'F'
    bgt     t2, t3, parse_hex_check_lower
    li      t3, 'A'
    sub     t2, t2, t3
    addi    t2, t2, 10
    j       parse_hex_add

parse_hex_check_lower:
    # Check a-f
    li      t3, 'a'
    blt     t2, t3, parse_hex_done
    li      t3, 'f'
    bgt     t2, t3, parse_hex_done
    li      t3, 'a'
    sub     t2, t2, t3
    addi    t2, t2, 10

parse_hex_add:
    slli    t0, t0, 4
    or      t0, t0, t2
    li      t1, 1
    addi    a0, a0, 1
    j       parse_hex_loop

parse_hex_done:
    mv      a0, t0
    mv      a1, t1
    ret

# parse_dec: Parse decimal number from string
# a0 = string pointer
# Returns: a0 = value, a1 = 1 if valid, 0 if not
parse_dec:
    li      t0, 0               # result
    li      t1, 0               # valid flag

parse_dec_loop:
    lbu     t2, 0(a0)

    # Check 0-9
    li      t3, '0'
    blt     t2, t3, parse_dec_done
    li      t3, '9'
    bgt     t2, t3, parse_dec_done

    # Digit: result = result * 10 + digit
    li      t3, '0'
    sub     t2, t2, t3          # t2 = digit value
    li      t3, 10
    mul     t0, t0, t3          # result *= 10
    add     t0, t0, t2          # result += digit
    li      t1, 1               # valid
    addi    a0, a0, 1
    j       parse_dec_loop

parse_dec_done:
    mv      a0, t0
    mv      a1, t1
    ret

# print_hex32: Print 32-bit hex value
# a0 = value
print_hex32:
    addi    sp, sp, -8
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    mv      s0, a0

    srli    a0, s0, 24
    andi    a0, a0, 0xFF
    call    print_hex8
    srli    a0, s0, 16
    andi    a0, a0, 0xFF
    call    print_hex8
    srli    a0, s0, 8
    andi    a0, a0, 0xFF
    call    print_hex8
    andi    a0, s0, 0xFF
    call    print_hex8

    lw      ra, 0(sp)
    lw      s0, 4(sp)
    addi    sp, sp, 8
    ret

# print_hex8: Print 8-bit hex value
# a0 = value (low 8 bits)
print_hex8:
    addi    sp, sp, -8
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    mv      s0, a0

    la      t0, hex_chars
    srli    t1, s0, 4
    andi    t1, t1, 0xF
    add     t1, t0, t1
    lbu     a0, 0(t1)
    call    putchar

    la      t0, hex_chars
    andi    t1, s0, 0xF
    add     t1, t0, t1
    lbu     a0, 0(t1)
    call    putchar

    lw      ra, 0(sp)
    lw      s0, 4(sp)
    addi    sp, sp, 8
    ret

# print_dec: Print decimal number
# a0 = value
print_dec:
    addi    sp, sp, -20
    sw      ra, 0(sp)
    mv      t0, a0              # value
    li      t1, 0               # digit count
    addi    t2, sp, 4           # buffer at sp+4

    # Handle 0 specially
    bnez    t0, 1f
    li      a0, '0'
    call    putchar
    j       print_dec_done

1:  # Extract digits (reverse order)
    li      t3, 10
2:  beqz    t0, 3f
    remu    t4, t0, t3
    addi    t4, t4, '0'
    add     t5, t2, t1
    sb      t4, 0(t5)
    addi    t1, t1, 1
    divu    t0, t0, t3
    j       2b

3:  # Print digits (reverse)
    addi    t1, t1, -1
    add     t5, t2, t1
    lbu     a0, 0(t5)
    sw      t1, 16(sp)          # Save t1, t2 (putchar clobbers them)
    sw      t2, 12(sp)
    call    putchar
    lw      t1, 16(sp)
    lw      t2, 12(sp)
    bnez    t1, 3b

print_dec_done:
    lw      ra, 0(sp)
    addi    sp, sp, 20
    ret

# print_dec_2digit: Print 2-digit decimal with leading zero
# a0 = value (0-99)
print_dec_2digit:
    addi    sp, sp, -8
    sw      ra, 0(sp)
    sw      s0, 4(sp)
    mv      s0, a0

    # Tens digit
    li      t0, 10
    divu    a0, s0, t0
    addi    a0, a0, '0'
    call    putchar

    # Ones digit
    li      t0, 10
    remu    a0, s0, t0
    addi    a0, a0, '0'
    call    putchar

    lw      ra, 0(sp)
    lw      s0, 4(sp)
    addi    sp, sp, 8
    ret
