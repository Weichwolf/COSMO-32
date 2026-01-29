10 REM Number guessing game
20 SECRET = (RND MOD 100) + 1
30 TRIES = 0
40 PRINT "Guess a number between 1 and 100"
50 INPUT "Your guess: ", GUESS
60 TRIES = TRIES + 1
70 IF GUESS < SECRET THEN PRINT "Too low!": GOTO 50
80 IF GUESS > SECRET THEN PRINT "Too high!": GOTO 50
90 PRINT "Correct! You got it in "; TRIES; " tries!"
100 END
