10 REM Test Numeric Functions (Integer-only)
20 PRINT "Integer Division (\):"
30 PRINT "17 \ 5 ="; 17 \ 5; "(expect 3)"
40 PRINT "100 \ 7 ="; 100 \ 7; "(expect 14)"
50 PRINT "-17 \ 5 ="; -17 \ 5; "(expect -3)"
60 PRINT
70 PRINT "Power (^):"
80 PRINT "2 ^ 8 ="; 2 ^ 8; "(expect 256)"
90 PRINT "3 ^ 4 ="; 3 ^ 4; "(expect 81)"
100 PRINT "10 ^ 3 ="; 10 ^ 3; "(expect 1000)"
110 PRINT "5 ^ 0 ="; 5 ^ 0; "(expect 1)"
120 PRINT
130 PRINT "SQR (Integer Square Root):"
140 PRINT "SQR(16) ="; SQR(16); "(expect 4)"
150 PRINT "SQR(17) ="; SQR(17); "(expect 4)"
160 PRINT "SQR(100) ="; SQR(100); "(expect 10)"
170 PRINT
180 PRINT "FIX (truncate toward 0):"
190 PRINT "FIX(7) ="; FIX(7); "(expect 7)"
200 PRINT "FIX(-7) ="; FIX(-7); "(expect -7)"
210 PRINT
220 PRINT "RANDOMIZE test:"
230 RANDOMIZE 12345
240 FOR I = 1 TO 5
250   PRINT RND;
260 NEXT I
270 PRINT
280 PRINT
290 PRINT "Combined:"
300 A = 2 ^ 4 \ 3
310 PRINT "2 ^ 4 \ 3 ="; A; "(expect 5)"
320 B = SQR(81) + 2 ^ 3
330 PRINT "SQR(81) + 2 ^ 3 ="; B; "(expect 17)"
340 PRINT
350 PRINT "Done."
360 END
