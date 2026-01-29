10 REM Graphics Drawing Demo
20 REM Requires: LINE with box, CIRCLE, PAINT, LOCATE, COLOR
30 CLS
40 PRINT "GRAPHICS DEMO"
50 PRINT "============="
60 PRINT "Press any key for each demo..."
70 A$ = INKEY$: IF A$ = "" THEN 70
80 CLS
90 REM Demo 1: Lines
100 LOCATE 1, 1: PRINT "Lines:"
110 FOR I = 0 TO 15
120   LINE (I * 20, 20)-(320, 200), I
130 NEXT I
140 A$ = INKEY$: IF A$ = "" THEN 140
150 CLS
160 REM Demo 2: Boxes
170 LOCATE 1, 1: PRINT "Boxes (B and BF):"
180 FOR I = 0 TO 7
190   LINE (I * 40, 30)-((I + 1) * 40 - 5, 100), I + 8, B
200   LINE (I * 40, 120)-((I + 1) * 40 - 5, 190), I + 8, BF
210 NEXT I
220 A$ = INKEY$: IF A$ = "" THEN 220
230 CLS
240 REM Demo 3: Circles
250 LOCATE 1, 1: PRINT "Circles:"
260 FOR I = 1 TO 10
270   CIRCLE (160, 100), I * 10, (I MOD 15) + 1
280 NEXT I
290 A$ = INKEY$: IF A$ = "" THEN 290
300 CLS
310 REM Demo 4: Filled shapes
320 LOCATE 1, 1: PRINT "Filled shapes with PAINT:"
330 CIRCLE (80, 100), 50, 15
340 PAINT (80, 100), 4, 15
350 LINE (180, 50)-(280, 150), 15, B
360 PAINT (230, 100), 2, 15
370 REM Triangle
380 LINE (350, 150)-(400, 50), 15
390 LINE (400, 50)-(450, 150), 15
400 LINE (450, 150)-(350, 150), 15
410 PAINT (400, 100), 6, 15
420 A$ = INKEY$: IF A$ = "" THEN 420
430 CLS
440 PRINT "Demo complete!"
450 END
