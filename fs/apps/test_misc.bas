10 REM Test Miscellaneous Features
20 PRINT "SWAP test:"
30 A = 10: B = 20
40 PRINT "Before: A="; A; " B="; B
50 SWAP A, B
60 PRINT "After:  A="; A; " B="; B
70 PRINT
80 A$ = "Hello": B$ = "World"
90 PRINT "Before: A$="; A$; " B$="; B$
100 SWAP A$, B$
110 PRINT "After:  A$="; A$; " B$="; B$
120 PRINT
130 PRINT "SLEEP test (2 seconds):"
140 PRINT "Waiting..."
150 SLEEP 2
160 PRINT "Done!"
170 PRINT
180 PRINT "BEEP test:"
190 BEEP
200 PRINT
210 PRINT "LINE INPUT test:"
220 PRINT "Enter a line with commas:"
230 LINE INPUT A$
240 PRINT "You entered: ["; A$; "]"
250 PRINT
260 PRINT "ERASE test:"
270 DIM X(10)
280 FOR I = 0 TO 10: X(I) = I * 2: NEXT I
290 PRINT "X(5) ="; X(5)
300 ERASE X
310 PRINT "Array erased"
320 END
