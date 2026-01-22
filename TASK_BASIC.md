# Aufgabe: BASIC-Interpreter auf QBasic 1.1 erweitern

## Ziel
Erweitere `os/src/basic.c` bis alle Testprogramme in `fs/apps/test_*.bas` fehlerfrei laufen.

## Kontext
- COSMO-32 ist ein Bare-Metal Emulator für RV32IMAC
- Der BASIC-Interpreter ist integer-only (kein Float)
- Feature-Liste: `fs/apps/FEATURES.txt`
- Bestehende Tests in `tests/custom/` müssen weiterhin bestehen (19 Tests)

## Aktueller Stand

### Abgeschlossen
- **Phase 1 - Kontrollfluss:** DO...LOOP (alle Varianten), EXIT DO/FOR, SELECT CASE, Block-IF
- **Phase 2 - Strings:** TAB, SPC, UCASE$, LCASE$, LTRIM$, RTRIM$, INSTR, SPACE$, STRING$, INPUT$, HEX$, OCT$
- **Phase 3 - Ausgabe:** LOCATE, COLOR, PRINT USING (Integer-Version)
- **Phase 4 - Subroutinen:** SUB/FUNCTION/CALL/DECLARE (rekursive Funktionen funktionieren)

### Offen
- **Phase 5 - Numerisch:** \, ^, RANDOMIZE, SQR, FIX
- **Phase 6 - Sonstiges:** SWAP, SLEEP, BEEP, LINE INPUT, ERASE, LINE (x1,y1)-(x2,y2)

## Nächste Schritte

Fortfahren mit Phase 5 (Numerisch):
```bash
# Test lesen
cat fs/apps/test_num.bas

# Nach Änderungen bauen und testen
make -C os && ./emu/build/cosmo32.exe --run-tests tests/custom/

# BASIC-Test ausführen
./emu/build/cosmo32.exe --headless os/firmware.bin --cmd "basic apps/test_num.bas" --timeout 5000
```

## Implementierungsdetails Phase 5

Numerische Operatoren:
1. `\` Integer-Division (wie / aber rundet ab)
2. `^` Potenz (integer via Schleife)
3. `RANDOMIZE [seed]` für RNG-Initialisierung
4. `SQR(n)` Integer-Wurzel
5. `FIX(n)` Abschneiden Richtung 0

## Vorgehen

### 1. Status prüfen
```bash
./emu/build/cosmo32.exe --run-tests tests/custom/
```

### 2. Testprogramm ausführen
```bash
./emu/build/cosmo32.exe --headless os/firmware.bin --cmd "basic apps/test_sub.bas" --timeout 5000
```

### 3. Nach jeder Änderung
```bash
make -C os && ./emu/build/cosmo32.exe --run-tests tests/custom/
```

## Phasen-Übersicht

**Phase 4 - Subroutinen:**
- `SUB name (params)...END SUB`
- `FUNCTION name (params)...END FUNCTION`
- `CALL name(args)` oder `name args`
- `DECLARE SUB/FUNCTION`
- `SHARED` für globale Variablen
- `EXIT SUB`, `EXIT FUNCTION`

**Phase 5 - Numerisch:**
- `\` Integer-Division
- `^` Potenz (integer)
- `RANDOMIZE [seed]`
- `SQR(n)` Integer-Wurzel
- `FIX(n)`

**Phase 6 - Sonstiges:**
- `SWAP a, b`
- `SLEEP n`
- `BEEP`
- `LINE INPUT [prompt;] var$`
- `ERASE arrayname`
- `LINE (x1,y1)-(x2,y2) [,color] [,B|BF]`

## Testprogramme
1. `test_do.bas` - DO...LOOP ✓
2. `test_if.bas` - Block-IF, EXIT FOR ✓
3. `test_sel.bas` - SELECT CASE ✓
4. `test_str.bas` - String-Funktionen ✓
5. `test_out.bas` - TAB, LOCATE, COLOR, PRINT USING ✓
6. `test_sub.bas` - SUB/FUNCTION ✓
7. `test_num.bas` - Numerische Operatoren (nächster Test)
8. `test_misc.bas` - SWAP, SLEEP, etc.
9. `test_line.bas` - LINE-Syntax

## Regeln
- Ein Feature nach dem anderen
- Nach jedem Feature: OS bauen + Tests laufen lassen
- Keine Änderungen an bestehenden Tests außer bei nachweislich falscher Spezifikation
- Bei Unklarheiten: fragen, nicht raten
- Integer-only: Trigonometrie/Float überspringen

## Dateien
- Interpreter: `os/src/basic.c`
- Display-Funktionen: `os/src/display.c`
- Konstanten: `os/src/const.h`, `os/src/config.h`
- Feature-Liste: `fs/apps/FEATURES.txt`
- Testprogramme: `fs/apps/test_*.bas`
