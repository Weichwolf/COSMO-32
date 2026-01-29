10 REM NOT operator and TIMER test
20 PRINT "Testing NOT operator:"
30 FALSE = 0
40 TRUE = 1
50 PRINT "FALSE="; FALSE; " NOT FALSE="; NOT FALSE
60 PRINT "TRUE="; TRUE; " NOT TRUE="; NOT TRUE
70 PRINT "NOT 0="; NOT 0
80 PRINT "NOT 1="; NOT 1
90 PRINT "NOT 5="; NOT 5
100 IF NOT FALSE THEN PRINT "NOT FALSE is true (correct)"
110 IF NOT TRUE THEN PRINT "NOT TRUE is true (wrong)"
120 PRINT "Testing TIMER:"
130 ELAPSED = TIMER
140 PRINT "Timer value: "; ELAPSED; " ms"
150 IF ELAPSED >= 0 THEN PRINT "Timer is running (correct)"
160 PRINT "Testing logical combinations:"
170 PRINT "(1 AND 1)="; (1 AND 1)
180 PRINT "(1 AND 0)="; (1 AND 0)
190 PRINT "(0 OR 1)="; (0 OR 1)
200 PRINT "(NOT 0 AND 1)="; (NOT 0 AND 1)
210 PRINT "Test complete!"
220 END
