# COSMO-32

RISC-V bare-metal platform: CH32V307 emulator (C++20) + retro OS with BASIC interpreter and UDP/TFTP networking.

## Hardware Target

- **CPU:** CH32V307 (RV32IMAC @ 144MHz)
- **Memory:** 64KB SRAM, 256KB Flash, 1MB external SRAM (FSMC)
- **Display:** 640x400 @ 4bpp or 320x200 @ 16bpp
- **Audio:** I2S stereo
- **Network:** 10M Ethernet with virtual DHCP/TFTP server

## Features

**Emulator**
- Full RV32IMAC instruction set (incl. compressed, atomics)
- Peripherals: USART, Timer, PFIC, DMA, FSMC, Display, I2S, Ethernet
- Built-in network services (ICMP Echo, DHCP, TFTP)
- Test runner for automated verification

**OS**
- Interactive shell with memory inspection tools
- UDP/TFTP network stack
- BASIC interpreter with arrays, strings, file I/O

## Build

Requires: RISC-V GCC toolchain, CMake 3.20+, Ninja, SDL2

```bash
# Emulator
cmake -B emu/build -G Ninja emu && ninja -C emu/build

# OS
make -C os
```

**MSYS2/Windows:** Use UCRT64 shell or set PATH:
```bash
export PATH="/c/msys64/ucrt64/bin:/usr/bin:/bin:$PATH"
```

## Run

```bash
# Interactive
./emu/build/cosmo32.exe os/firmware.bin

# Run tests (19 CPU + peripheral tests)
./emu/build/cosmo32.exe --run-tests tests/custom/

# Headless with command
./emu/build/cosmo32.exe --headless os/firmware.bin --cmd "basic apps/hello.bas" --timeout 5000
```

## Shell Commands

| Command | Description |
|---------|-------------|
| `help` | List all commands |
| `mem <addr> [len]` | Dump memory (hex) |
| `peek <addr>` | Read byte |
| `poke <addr> <val>` | Write byte |
| `regs` | Show CPU registers |
| `ping` | ICMP echo test |
| `tftp get <file>` | Download file |
| `tftp put <file>` | Upload file |
| `basic [file]` | Run BASIC interpreter |

## Project Structure

```
COSMO-32/
├── emu/                 # Emulator (C++20)
│   └── src/
│       ├── cpu.cpp/hpp  # RV32IMAC CPU
│       ├── bus.hpp      # Memory-mapped I/O
│       ├── decode.hpp   # Instruction decoder
│       └── device/      # Peripherals
├── os/                  # Operating System
│   └── src/
│       ├── boot.S       # Startup code
│       ├── shell.S      # Shell + I/O
│       ├── net.S        # Network stack
│       └── basic.c      # BASIC interpreter
├── tests/custom/        # Hardware tests
└── fs/                  # TFTP filesystem
```

## Virtual Network

Server: `10.0.0.1` / Client: `10.0.0.2`

| Service | Port | Description |
|---------|------|-------------|
| ICMP | - | Ping |
| DHCP | 67/68 | IP assignment |
| TFTP | 69 | File transfer (`/.dir` for listing) |

## Status

- Emulator: CPU, all peripherals functional (19 tests pass)
- OS: Shell, networking, BASIC operational
- Pending: SDL2 frontend, GDB stub, TAP backend

## License

Apache-2.0
