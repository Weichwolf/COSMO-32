# Emulator Performance Optimierung

Aktuell: **158 MIPS** (Interpreter, kein JIT)

Ziel: **300+ MIPS** ohne JIT-Komplexität

## Ansätze nach Aufwand/Ertrag

### 1. Computed Gotos (Threaded Code)

**Aufwand:** 2h | **Erwartung:** +35%

```cpp
// Statt switch(opcode) - direkter Sprung
static void* dispatch[128] = { &&ADD, &&SUB, &&LW, ... };
ADD: r[rd] = r[rs1] + r[rs2]; goto *dispatch[decode(*pc++)];
SUB: r[rd] = r[rs1] - r[rs2]; goto *dispatch[decode(*pc++)];
```

Switch ist ein indirekter Jump der ständig misspredicted wird. Threaded Code: jeder Handler endet mit eigenem Jump → CPU lernt die Patterns pro Instruktion.

### 2. Profile-Guided Optimization (PGO)

**Aufwand:** 10min | **Erwartung:** +20%

```bash
# 1. Instrumentierter Build
cmake -DCMAKE_CXX_FLAGS="-fprofile-generate" ..
./cosmo32 --run-tests tests/custom/

# 2. Optimierter Build mit Profildaten
cmake -DCMAKE_CXX_FLAGS="-fprofile-use" ..
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

### 4. Lazy Flag Evaluation

**Aufwand:** 3h | **Erwartung:** +20%

```cpp
// Nicht:
flags.zero = (result == 0);
flags.neg = (result < 0);

// Sondern:
last_op = OP_ADD;
last_a = a; last_b = b; last_result = result;

// Erst bei Branch evaluieren:
bool IsZero() { return last_result == 0; }
```

~80% der Flag-Berechnungen werden nie gelesen.

### 5. Superblock Interpretation

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

### 6. Partial Evaluation (Futamura-Projektion)

**Aufwand:** 20h+ | **Erwartung:** +100-200%

Interpreter spezialisiert auf *diese* Firmware:

- Fetch entfällt (Instruktion ist bekannt)
- Decode entfällt (Opcode ist bekannt)
- Nur Execute bleibt

Der Interpreter wird zum Compiler durch Spezialisierung auf das Programm.

## Roadmap

| Phase | Ansatz | Kumulativ |
|-------|--------|-----------|
| 1 | PGO | ~190 MIPS |
| 2 | Computed Gotos | ~255 MIPS |
| 3 | Instruction Fusion | ~320 MIPS |
| 4 | Superblocks | ~480 MIPS |

## Nicht-Ziele

- JIT-Kompilierung (zu komplex, plattformabhängig)
- LLVM/Cranelift Integration (externe Abhängigkeit)
- GPU-Beschleunigung (sequentielle Ausführung)
