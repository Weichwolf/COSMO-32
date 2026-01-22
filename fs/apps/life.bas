10 REM Conway's Game of Life
20 REM Requires: 2D arrays, LOCATE, DO..LOOP
30 CLS
40 W = 40: H = 20
50 DIM G(42, 22), N(42, 22)
60 PRINT "CONWAY'S GAME OF LIFE"
70 PRINT "Press R for Random, G for Glider, Q to quit"
80 K$ = INKEY$: IF K$ = "" THEN 80
90 IF K$ = "q" OR K$ = "Q" THEN END
100 CLS
110 REM Initialize
120 IF K$ = "r" OR K$ = "R" THEN GOSUB 500
130 IF K$ = "g" OR K$ = "G" THEN GOSUB 600
140 GEN = 0
150 REM Main loop
160 DO
170   REM Display
180   FOR Y = 1 TO H
190     LOCATE Y, 1
200     FOR X = 1 TO W
210       IF G(X, Y) = 1 THEN PRINT "*"; ELSE PRINT " ";
220     NEXT X
230   NEXT Y
240   GEN = GEN + 1
250   LOCATE 22, 1: PRINT "Generation:"; GEN; "  Press Q to quit"
260   REM Calculate next generation
270   FOR Y = 1 TO H
280     FOR X = 1 TO W
290       C = 0
300       FOR DY = -1 TO 1
310         FOR DX = -1 TO 1
320           IF DX = 0 AND DY = 0 THEN 340
330           C = C + G(X + DX, Y + DY)
340         NEXT DX
350       NEXT DY
360       IF G(X, Y) = 1 THEN
370         IF C < 2 OR C > 3 THEN N(X, Y) = 0 ELSE N(X, Y) = 1
380       ELSE
390         IF C = 3 THEN N(X, Y) = 1 ELSE N(X, Y) = 0
400       END IF
410     NEXT X
420   NEXT Y
430   REM Copy next to current
440   FOR Y = 1 TO H
450     FOR X = 1 TO W
460       G(X, Y) = N(X, Y)
470     NEXT X
480   NEXT Y
490   K$ = INKEY$: IF K$ = "q" OR K$ = "Q" THEN EXIT DO
495   SLEEP 1
497 LOOP
498 END
500 REM Random initialization
510 RANDOMIZE TIMER
520 FOR Y = 1 TO H
530   FOR X = 1 TO W
540     IF RND > 0.7 THEN G(X, Y) = 1 ELSE G(X, Y) = 0
550   NEXT X
560 NEXT Y
570 RETURN
600 REM Glider
610 G(5, 3) = 1
620 G(6, 4) = 1
630 G(4, 5) = 1: G(5, 5) = 1: G(6, 5) = 1
640 RETURN
