10 REM Mandelbrot - Fixpoint (scale=256)
20 REM 320x200 centered, 16 colors
30 CLS
40 T0 = TIMER
50 SC = 256
60 MAXI = 20
70 W = 320
80 H = 200
90 FOR PY = 0 TO H - 1
100 FOR PX = 0 TO W - 1
110 REM Map pixel to complex plane
120 REM cx = -2.0 + px * 3.0 / W  (in fixpoint: -512 + px * 768 / W)
130 REM cy = -1.0 + py * 2.0 / H  (in fixpoint: -256 + py * 512 / H)
140 CX = -512 + (PX * 768) / W
150 CY = -256 + (PY * 512) / H
160 ZX = 0
170 ZY = 0
180 I = 0
190 REM Iterate: z = z^2 + c
200 ZX2 = (ZX * ZX) / SC
210 ZY2 = (ZY * ZY) / SC
220 IF ZX2 + ZY2 > 1024 THEN GOTO 280
230 IF I >= MAXI THEN GOTO 280
240 ZN = ZX2 - ZY2 + CX
250 ZY = (2 * ZX * ZY) / SC + CY
260 ZX = ZN
270 I = I + 1 : GOTO 200
280 C = I MOD 16
290 PSET PX, PY, C
300 NEXT PX
310 NEXT PY
320 END
