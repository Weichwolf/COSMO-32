10 REM Test SUB and FUNCTION
20 DECLARE SUB PrintBox (W, H)
30 DECLARE FUNCTION Factorial (N)
40 DECLARE FUNCTION Max (A, B)
50 PRINT "Test SUB:"
60 CALL PrintBox(5, 3)
70 PRINT
80 PRINT "Test FUNCTION:"
90 PRINT "Factorial(5) ="; Factorial(5)
100 PRINT "Max(7, 12) ="; Max(7, 12)
110 END
120 SUB PrintBox (W, H)
130   FOR Y = 1 TO H
140     FOR X = 1 TO W
150       IF Y = 1 OR Y = H OR X = 1 OR X = W THEN
160         PRINT "*";
170       ELSE
180         PRINT " ";
190       END IF
200     NEXT X
210     PRINT
220   NEXT Y
230 END SUB
240 FUNCTION Factorial (N)
250   IF N <= 1 THEN
260     Factorial = 1
270   ELSE
280     Factorial = N * Factorial(N - 1)
290   END IF
300 END FUNCTION
310 FUNCTION Max (A, B)
320   IF A > B THEN
330     Max = A
340   ELSE
350     Max = B
360   END IF
370 END FUNCTION
