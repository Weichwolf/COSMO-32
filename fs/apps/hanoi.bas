10 REM Towers of Hanoi - Recursive Solution
20 REM Requires: SUB, recursive calls
30 DECLARE SUB Hanoi (N, SRC$, DST$, TMP$)
40 CLS
50 PRINT "TOWERS OF HANOI"
60 PRINT "==============="
70 PRINT
80 INPUT "Enter number of disks (1-10): ", D
90 IF D < 1 OR D > 10 THEN 80
100 PRINT
110 PRINT "Moving"; D; "disks from A to C:"
120 PRINT
130 M = 0
140 CALL Hanoi(D, "A", "C", "B")
150 PRINT
160 PRINT "Total moves:"; M
170 PRINT "Minimum moves: "; 2 ^ D - 1
180 END
190 SUB Hanoi (N, SRC$, DST$, TMP$)
200   SHARED M
210   IF N = 0 THEN EXIT SUB
220   CALL Hanoi(N - 1, SRC$, TMP$, DST$)
230   M = M + 1
240   PRINT "Move disk"; N; "from"; SRC$; "to"; DST$
250   CALL Hanoi(N - 1, TMP$, DST$, SRC$)
260 END SUB
