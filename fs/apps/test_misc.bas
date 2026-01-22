10 REM Test Miscellaneous Features (headless-compatible)
20 PRINT "SWAP test:"
30 A = 10: B = 20
40 PRINT "Before: A="; A; " B="; B
50 SWAP A, B
60 PRINT "After:  A="; A; " B="; B; "(expect 20, 10)"
70 PRINT
80 A$ = "Hello": B$ = "World"
90 PRINT "Before: A$="; A$; " B$="; B$
100 SWAP A$, B$
110 PRINT "After:  A$="; A$; " B$="; B$; "(expect World, Hello)"
120 PRINT
130 PRINT "SLEEP test:"
140 PRINT "Sleeping 0 seconds..."
150 SLEEP 0
160 PRINT "Done!"
170 PRINT
180 PRINT "BEEP test:"
190 BEEP
195 PRINT "(no sound in emulator)"
200 PRINT
210 REM LINE INPUT skipped - requires keyboard
220 PRINT "ERASE test:"
230 DIM X(10)
240 FOR I = 0 TO 10
250 X(I) = I * 2
260 NEXT I
270 PRINT "X(5) before ERASE ="; X(5); "(expect 10)"
280 ERASE X
290 PRINT "X(5) after ERASE ="; X(5); "(expect 0)"
300 PRINT
310 PRINT "All tests passed."
320 END
