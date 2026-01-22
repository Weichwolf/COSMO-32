10 REM NIBBLES - Snake Game (QBasic style)
20 REM Requires: DO..LOOP, LOCATE, COLOR, INKEY$, TIMER, arrays
30 CLS
40 DIM B(100, 2)
50 PRINT "NIBBLES - Use WASD to move, Q to quit"
60 PRINT "Press any key to start..."
70 A$ = INKEY$: IF A$ = "" THEN 70
80 CLS
90 REM Draw border
100 COLOR 7
110 FOR X = 1 TO 40
120   LOCATE 1, X: PRINT "-"
130   LOCATE 24, X: PRINT "-"
140 NEXT X
150 FOR Y = 1 TO 24
160   LOCATE Y, 1: PRINT "|"
170   LOCATE Y, 40: PRINT "|"
180 NEXT Y
190 REM Initialize snake
200 SX = 20: SY = 12
210 SL = 3
220 DX = 1: DY = 0
230 FOR I = 1 TO SL
240   B(I, 1) = SX - I + 1
250   B(I, 2) = SY
260 NEXT I
270 REM Place food
280 FX = INT(RND * 36) + 3
290 FY = INT(RND * 20) + 3
300 COLOR 12
310 LOCATE FY, FX: PRINT "*"
320 REM Main game loop
330 SC = 0
340 DO
350   REM Get input
360   K$ = INKEY$
370   IF K$ = "w" OR K$ = "W" THEN DX = 0: DY = -1
380   IF K$ = "s" OR K$ = "S" THEN DX = 0: DY = 1
390   IF K$ = "a" OR K$ = "A" THEN DX = -1: DY = 0
400   IF K$ = "d" OR K$ = "D" THEN DX = 1: DY = 0
410   IF K$ = "q" OR K$ = "Q" THEN EXIT DO
420   REM Move snake
430   NX = B(1, 1) + DX
440   NY = B(1, 2) + DY
450   REM Check collision with wall
460   IF NX < 2 OR NX > 39 OR NY < 2 OR NY > 23 THEN EXIT DO
470   REM Check self collision
480   FOR I = 1 TO SL
490     IF B(I, 1) = NX AND B(I, 2) = NY THEN EXIT DO
500   NEXT I
510   REM Erase tail
520   COLOR 0
530   LOCATE B(SL, 2), B(SL, 1): PRINT " "
540   REM Shift body
550   FOR I = SL TO 2 STEP -1
560     B(I, 1) = B(I - 1, 1)
570     B(I, 2) = B(I - 1, 2)
580   NEXT I
590   B(1, 1) = NX
600   B(1, 2) = NY
610   REM Draw head
620   COLOR 10
630   LOCATE NY, NX: PRINT "O"
640   REM Check food
650   IF NX = FX AND NY = FY THEN
660     SC = SC + 10
670     SL = SL + 1
680     B(SL, 1) = B(SL - 1, 1)
690     B(SL, 2) = B(SL - 1, 2)
700     FX = INT(RND * 36) + 3
710     FY = INT(RND * 20) + 3
720     COLOR 12
730     LOCATE FY, FX: PRINT "*"
740   END IF
750   REM Show score
760   COLOR 15
770   LOCATE 25, 1: PRINT "Score:"; SC
780   REM Delay
790   T = TIMER
800   DO WHILE TIMER - T < 0.1
810   LOOP
820 LOOP
830 COLOR 15
840 LOCATE 12, 15: PRINT "GAME OVER!"
850 LOCATE 13, 13: PRINT "Final Score:"; SC
860 END
