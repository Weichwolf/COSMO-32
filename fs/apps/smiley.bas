10 REM Acid Smiley
20 REM Yellow = 14, Black = 0
30 CLS
40 REM Face (yellow filled circle)
50 FCIRCLE 320, 200, 100, 14
60 REM Left eye
70 FCIRCLE 280, 170, 15, 0
80 REM Right eye
90 FCIRCLE 360, 170, 15, 0
100 REM Mouth - parabola grin (smile up!)
110 FOR X = -60 TO 60
120 Y = (X * X) / 120
130 FOR T = 0 TO 4
140 PSET 320 + X, 260 - Y + T, 0
150 NEXT T
160 NEXT X
170 END
