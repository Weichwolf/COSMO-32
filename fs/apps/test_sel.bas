10 REM Test SELECT CASE
20 FOR N = 1 TO 7
30   PRINT "N="; N; " -> ";
40   SELECT CASE N
50     CASE 1
60       PRINT "one"
70     CASE 2, 3
80       PRINT "two or three"
90     CASE 4 TO 6
100      PRINT "four to six"
110    CASE ELSE
120      PRINT "other"
130   END SELECT
140 NEXT N
150 PRINT
160 PRINT "String test:"
170 A$ = "hello"
180 SELECT CASE A$
190   CASE "hi"
200     PRINT "greeting hi"
210   CASE "hello"
220     PRINT "greeting hello"
230   CASE ELSE
240     PRINT "unknown"
250 END SELECT
260 END
