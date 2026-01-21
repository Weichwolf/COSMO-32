# COSMO-32

Bare-metal Entwicklungsplattform. Emulator zuerst, Hardware folgt.

## Hardware-Ziel

CH32V307 (RV32IMAC @ 144MHz), 64KB SRAM, 256KB Flash, 1MB externes SRAM via FSMC.
Display 640×400×4bpp oder 320×200×16bpp, I2S Audio, 10M Ethernet.

## Projektstruktur

```
COSMO-32/
├── emu/                          # Emulator (C++20)
│   ├── src/
│   │   ├── main.cpp              # EmulatorContext, Test-Runner, CLI
│   │   ├── cpu.hpp/cpp           # RV32IMAC CPU mit Fehler-Logging
│   │   ├── bus.hpp               # Device Interface, Bus mit Zugriffs-Logging
│   │   ├── decode.hpp            # Opcodes, C-Extension Expansion (dokumentiert)
│   │   └── device/
│   │       ├── memory.hpp        # RAM/ROM
│   │       ├── usart.hpp         # USART (RX-Queue limitiert)
│   │       ├── timer.hpp         # SysTick Timer
│   │       ├── pfic.hpp          # Interrupt Controller
│   │       ├── dma.hpp           # DMA Controller
│   │       ├── fsmc.hpp          # External SRAM (1MB)
│   │       ├── display.hpp       # Display Control
│   │       ├── i2s.hpp           # I2S Audio
│   │       └── eth.hpp           # Ethernet + DHCP/TFTP (PacketBuilder)
│   └── CMakeLists.txt
├── os/                           # COSMO-32 OS (ASM/C)
│   ├── src/
│   │   ├── const.h               # Hardware-Konstanten (Memory Map, Register)
│   │   ├── config.h              # Konfiguration (IP, Ports, Limits)
│   │   ├── boot.S                # Boot-Code
│   │   ├── main.S                # Main Entry
│   │   ├── shell.S               # Shell + I/O
│   │   ├── net.S                 # UDP/TFTP Client
│   │   ├── basic.c               # BASIC-Interpreter
│   │   ├── display.c             # Display-Treiber (80x50 Terminal)
│   │   └── font8x8.h             # 8x8 Bitmap-Font (CP437-Style)
│   ├── link.ld
│   └── Makefile
├── tests/custom/                 # CPU + Peripheral Tests (19 Tests)
│   ├── *.s
│   └── Makefile
└── fs/                           # TFTP-Dateisystem (Host)
    └── apps/*.bas                # BASIC-Programme
```

## Build

```bash
cmake -B emu/build -G Ninja emu && ninja -C emu/build   # Emulator
make -C os                                               # OS
```

## Ausführen

```bash
./emu/build/cosmo32.exe os/firmware.bin                  # Interaktiv (SDL2 Fenster)
./emu/build/cosmo32.exe --run-tests tests/custom/        # 19 Tests
./emu/build/cosmo32.exe --headless os/firmware.bin --cmd "help" --timeout 2000
./emu/build/cosmo32.exe --headless os/firmware.bin --timeout 500 --screenshot out.ppm
```

Shell-Befehle: `help` zeigt alle. BASIC: `basic` (interaktiv) oder `basic apps/file.bas`.

## PATH (kritisch!)

Außerhalb UCRT64-Shell: `export PATH="/c/msys64/ucrt64/bin:/usr/bin:/bin:$PATH"`

Sonst ABI-Mismatch (MINGW64-DLLs), Compiler scheitert mit Exit 1 ohne Output.

## Eingebaute Netzwerk-Services

Virtuelles Netzwerk: Server 10.0.0.1, Client 10.0.0.2

| Service | Port | Beschreibung |
|---------|------|--------------|
| ICMP Echo | - | Ping |
| DHCP | 67/68 | IP-Vergabe |
| TFTP | 69 | Dateisystem R/W, `/.dir` für Listing |

## Debug-Features

Logging bei Fehlern (stderr):
- `[CPU] Illegal instruction at PC=0x... : 0x...`
- `[CPU] Unknown CSR read/write: 0x... at PC=0x...`
- `[BUS] Unmapped read/write: 0x...`

## Status

**Emulator:** CPU, Bus, USART, Timer, PFIC, DMA, FSMC, Display, I2S, ETH, SDL2 Frontend (19 Tests pass)

**OS:** Shell, Netzwerk-Stack (UDP/TFTP), BASIC-Interpreter, Display-Treiber (80×50 Terminal)

**Offen:** GDB Stub, TAP-Backend, Audio-Output (I2S→SDL vorbereitet)

## Architektur-Entscheidungen

- `EmulatorContext` zentralisiert Device-Setup (keine Code-Duplikation)
- `PacketBuilder` für ETH-Pakete mit Bounds-Checks (assert)
- USART RX-Queue auf 4KB limitiert (verhindert Memory-Overflow)
- decode.hpp vollständig dokumentiert (RV32C Instruction Expansion)
- Display: Dual-Output via `putchar` (USART + Framebuffer), 4bpp mit CGA-Palette
- Linker-Script inkludiert `.sdata/.sbss` für RISC-V small data sections

## Nicht-Ziele

- Cycle-Accuracy
- Vollständige CH32V307-Peripherie
- TCP-Stack im Guest OS (UDP + Emulator-Proxy stattdessen)
