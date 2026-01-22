10 REM Test String Functions
20 A$ = "  Hello World  "
30 PRINT "Original: ["; A$; "]"
40 PRINT
50 PRINT "UCASE$: ["; UCASE$(A$); "]"
60 PRINT "LCASE$: ["; LCASE$(A$); "]"
70 PRINT "LTRIM$: ["; LTRIM$(A$); "]"
80 PRINT "RTRIM$: ["; RTRIM$(A$); "]"
90 B$ = LTRIM$(RTRIM$(A$))
100 PRINT "Trimmed: ["; B$; "]"
110 PRINT
120 PRINT "INSTR tests:"
130 PRINT "INSTR(A$, 'World') ="; INSTR(A$, "World")
140 PRINT "INSTR(A$, 'xyz') ="; INSTR(A$, "xyz")
150 PRINT "INSTR(5, A$, 'o') ="; INSTR(5, A$, "o")
160 PRINT
170 PRINT "SPACE$ and STRING$:"
180 PRINT "["; SPACE$(5); "]"
190 PRINT "["; STRING$(10, "*"); "]"
200 PRINT "["; STRING$(5, 65); "]"
210 PRINT
220 PRINT "HEX$ and OCT$:"
230 PRINT "HEX$(255) = "; HEX$(255)
240 PRINT "OCT$(64) = "; OCT$(64)
250 PRINT
260 PRINT "INPUT$ test (press 3 keys):"
270 K$ = INPUT$(3)
280 PRINT "You typed: ["; K$; "]"
290 END
