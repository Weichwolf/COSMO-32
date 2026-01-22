# Aufgabe: BASIC-Interpreter auf QBasic 1.1 erweitern

## Anweisung nach /clear

Lies diese Datei und setze die Implementierung bei der nächsten offenen Phase fort.

```bash
# 1. Aktuellen Test ausführen um Stand zu prüfen
./emu/build/cosmo32.exe --headless os/firmware.bin --cmd "basic apps/test_num.bas" --timeout 5000

# 2. Nach Änderungen bauen und alle Tests laufen lassen
make -C os && ./emu/build/cosmo32.exe --run-tests tests/custom/
```

## Aktueller Stand

| Phase | Beschreibung | Status |
|-------|--------------|--------|
| 1 | Kontrollfluss: DO...LOOP, EXIT DO/FOR, SELECT CASE, Block-IF | ✓ |
| 2 | Strings: TAB, SPC, UCASE$, LCASE$, LTRIM$, RTRIM$, INSTR, SPACE$, STRING$, INPUT$, HEX$, OCT$ | ✓ |
| 3 | Ausgabe: LOCATE, COLOR, PRINT USING | ✓ |
| 4 | Subroutinen: SUB/FUNCTION/CALL/DECLARE (rekursiv) | ✓ |
| 5 | Numerisch: `\`, `^`, RANDOMIZE, SQR, FIX | **offen** |
| 6 | Sonstiges: SWAP, SLEEP, BEEP, LINE INPUT, ERASE, LINE-Grafik | offen |

## Nächste Phase: 5 - Numerisch

Testprogramm: `fs/apps/test_num.bas`

Zu implementieren in `os/src/basic.c`:

| Feature | Beschreibung | Wo |
|---------|--------------|-----|
| `\` | Integer-Division (rundet Richtung 0) | `term()` |
| `^` | Potenz (integer, via Schleife) | `term()` oder neuer Precedence-Level |
| `RANDOMIZE [seed]` | RNG-Initialisierung | neues Statement |
| `SQR(n)` | Integer-Wurzel | `factor()` |
| `FIX(n)` | Abschneiden Richtung 0 (bei int = identity) | `factor()` |

## Testprogramme

| Datei | Inhalt | Status |
|-------|--------|--------|
| test_do.bas | DO...LOOP | ✓ |
| test_if.bas | Block-IF, EXIT FOR | ✓ |
| test_sel.bas | SELECT CASE | ✓ |
| test_str.bas | String-Funktionen | ✓ |
| test_out.bas | TAB, LOCATE, COLOR, PRINT USING | ✓ |
| test_sub.bas | SUB/FUNCTION | ✓ |
| test_num.bas | Numerische Operatoren | **nächster** |
| test_misc.bas | SWAP, SLEEP, etc. | offen |
| test_line.bas | LINE-Grafik | offen |

## Regeln

- Ein Feature nach dem anderen
- Nach jedem Feature: `make -C os && ./emu/build/cosmo32.exe --run-tests tests/custom/`
- 19 CPU-Tests müssen weiterhin bestehen
- Integer-only: kein Float, keine Trigonometrie
- Bei Unklarheiten: fragen

## Dateien

- `os/src/basic.c` - Interpreter (Hauptdatei)
- `os/src/display.c` - Display-Funktionen
- `fs/apps/FEATURES.txt` - Feature-Liste
- `fs/apps/test_*.bas` - Testprogramme
