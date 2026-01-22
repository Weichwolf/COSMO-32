10 DECLARE SUB Box (W, H)
20 CALL Box(3, 2)
30 END
40 SUB Box (W, H)
50   FOR Y = 1 TO H
60     FOR X = 1 TO W
70       IF Y = 1 OR Y = H OR X = 1 OR X = W THEN
80         PRINT "*";
90       ELSE
100        PRINT " ";
110      END IF
120    NEXT X
130    PRINT
140  NEXT Y
150 END SUB
