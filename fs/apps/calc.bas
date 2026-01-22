10 REM Simple Calculator
20 REM Requires: SELECT CASE, DO..LOOP, numeric functions
30 CLS
40 PRINT "SIMPLE CALCULATOR"
50 PRINT "================="
60 PRINT
70 PRINT "Operations: + - * / \ ^ MOD SQR"
80 PRINT "Enter 'Q' to quit"
90 PRINT
100 DO
110   LINE INPUT "Expression: "; E$
120   IF UCASE$(E$) = "Q" THEN EXIT DO
130   IF E$ = "" THEN 110
140   REM Parse simple expression: num op num
150   REM Find operator
160   OP$ = ""
170   FOR I = 2 TO LEN(E$)
180     C$ = MID$(E$, I, 1)
190     SELECT CASE C$
200       CASE "+", "-", "*", "/", "\", "^"
210         OP$ = C$
220         P = I
230         EXIT FOR
240       CASE "M", "m"
250         IF UCASE$(MID$(E$, I, 3)) = "MOD" THEN
260           OP$ = "MOD"
270           P = I
280           EXIT FOR
290         END IF
300       CASE "S", "s"
310         IF UCASE$(MID$(E$, I, 3)) = "SQR" THEN
320           OP$ = "SQR"
330           P = I
340           EXIT FOR
350         END IF
360     END SELECT
370   NEXT I
380   IF OP$ = "" THEN
390     PRINT "Unknown operation"
400     GOTO 110
410   END IF
420   A = VAL(LEFT$(E$, P - 1))
430   IF OP$ = "SQR" THEN
440     PRINT "= "; SQR(A)
450     GOTO 110
460   END IF
470   IF OP$ = "MOD" THEN
480     B = VAL(MID$(E$, P + 3))
490   ELSE
500     B = VAL(MID$(E$, P + 1))
510   END IF
520   SELECT CASE OP$
530     CASE "+"
540       R = A + B
550     CASE "-"
560       R = A - B
570     CASE "*"
580       R = A * B
590     CASE "/"
600       IF B = 0 THEN PRINT "Division by zero": GOTO 110
610       R = A / B
620     CASE "\"
630       IF B = 0 THEN PRINT "Division by zero": GOTO 110
640       R = A \ B
650     CASE "^"
660       R = A ^ B
670     CASE "MOD"
680       IF B = 0 THEN PRINT "Division by zero": GOTO 110
690       R = A MOD B
700   END SELECT
710   PRINT "= "; R
720 LOOP
730 PRINT "Goodbye!"
740 END
