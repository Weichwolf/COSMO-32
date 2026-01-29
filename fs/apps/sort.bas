10 REM Sorting Demo - Bubble Sort with visualization
20 REM Requires: DO..LOOP, SWAP, LOCATE, arrays
30 CLS
40 PRINT "BUBBLE SORT VISUALIZATION"
50 PRINT "========================="
60 PRINT
70 N = 20
80 DIM A(20)
90 REM Initialize with random values
100 RANDOMIZE TIMER
110 FOR I = 1 TO N
120   A(I) = INT(RND * 50) + 1
130 NEXT I
140 PRINT "Original array:"
150 GOSUB 400
160 PRINT
170 PRINT "Sorting..."
180 PRINT
190 REM Bubble Sort
200 SWAPS = 0
210 DO
220   DONE = 1
230   FOR I = 1 TO N - 1
240     IF A(I) > A(I + 1) THEN
250       SWAP A(I), A(I + 1)
260       DONE = 0
270       SWAPS = SWAPS + 1
280       REM Show progress
290       LOCATE 10, 1
300       GOSUB 400
310     END IF
320   NEXT I
330 LOOP UNTIL DONE = 1
340 PRINT
350 PRINT "Sorted!"
360 PRINT "Total swaps:"; SWAPS
370 GOSUB 400
380 END
390 REM Print array subroutine
400 FOR I = 1 TO N
410   PRINT USING "##"; A(I);
420 NEXT I
430 PRINT
440 RETURN
