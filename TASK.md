# Emulator Performance Optimierung

Aktuell: **175 MIPS** (Release + LTO, Baseline)

Ziel: **350+ MIPS** ohne JIT-Komplexität

Mit PGO: **248 MIPS** (+42%)

## Ansätze nach Aufwand/Ertrag

### 1. Computed Gotos (Threaded Code)

**Aufwand:** 2h | **Ergebnis:** Verworfen ✗

GCC 15 mit LTO generiert bereits optimale Jump-Tabellen für Switch. Computed Gotos brachten:
- Ohne PGO: 175 → 175 MIPS (kein Unterschied)
- Mit PGO: 248 → 204 MIPS (18% LANGSAMER!)

Computed Gotos stören die PGO-Optimierungen. Der Compiler weiß es besser.

### 2. Profile-Guided Optimization (PGO)

**Aufwand:** 10min | **Ergebnis:** +42% (175 → 248 MIPS) ✓

```bash
# 1. Instrumentierter Build
cmake -B emu/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_PGO_GEN=ON emu
ninja -C emu/build
./emu/build/cosmo32.exe --run-tests tests/custom/
./emu/build/cosmo32.exe --headless os/firmware.bin --cmd "bench" --timeout 20000

# 2. Optimierter Build mit Profildaten
cmake -B emu/build -DENABLE_PGO_GEN=OFF -DENABLE_PGO_USE=ON -DENABLE_LTO=ON emu
ninja -C emu/build
```

Compiler optimiert Hot-Paths basierend auf echten Laufzeitdaten.

### 3. Instruction Fusion

**Aufwand:** 4h | **Erwartung:** +25%

Häufige Sequenzen als einzelne Handler:

| Sequenz | Fusion |
|---------|--------|
| `LUI + ADDI` | `LOAD_IMMEDIATE` |
| `ADDI sp + SW` | `PUSH` |
| `LW + ADDI sp` | `POP` |
| `BEQ + J` | `BRANCH_FAR` |

Firmware offline scannen, Fusionen im Decoder markieren.

### 4. Superblock Interpretation

**Aufwand:** 8h | **Erwartung:** +50%

Basic Blocks als einzelne Funktionen, kein Dispatch innerhalb:

```cpp
void block_0x1000() {
    r[1] = r[2] + r[3];      // ADD
    r[4] = r[5] + r[6];      // ADD
    mem[r[7]+8] = r[8];      // SW
    if (r[9] != r[10])       // BNE
        return block_0x1020();
    return block_0x1010();
}
```

Offline generiert aus Firmware-Analyse. Kein JIT, aber ähnliche Vorteile.

### 5. Partial Evaluation (Futamura-Projektion)

**Aufwand:** 20h+ | **Erwartung:** +100-200%

Interpreter spezialisiert auf *diese* Firmware:

- Fetch entfällt (Instruktion ist bekannt)
- Decode entfällt (Opcode ist bekannt)
- Nur Execute bleibt

Der Interpreter wird zum Compiler durch Spezialisierung auf das Programm.

## Testen

```bash
export PATH="/c/msys64/ucrt64/bin:/usr/bin:/bin:$PATH"
./emu/build/cosmo32.exe --headless os/firmware.bin --cmd "bench" --timeout 15000
```

## Roadmap

| Phase | Ansatz | Kumulativ |
|-------|--------|-----------|
| 1 | PGO | **248 MIPS** ✓ |
| 2 | Computed Gotos | Verworfen ✗ |
| 3 | Instruction Fusion | ~310 MIPS (?) |
| 4 | Superblocks | ~470 MIPS (?) |

## Nicht-Ziele

- JIT-Kompilierung (zu komplex, plattformabhängig)
- LLVM/Cranelift Integration (externe Abhängigkeit)
- GPU-Beschleunigung (sequentielle Ausführung)
