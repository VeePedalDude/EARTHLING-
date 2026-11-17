# A short "player" session on a Terrarium: set a sound, tweak, stomp, tweak again.
board terrarium
set ALLPOTS 0.5
set POT1 0.3                 # delay time
at 0.8 ramp 0.8 1.6 POT2 0.3 0.8   # bring feedback up while playing
at 2.0 tap FS1 60ms          # bypass
at 2.6 tap FS1 60ms          # engage again
at 3.0 toggle SW1 on
at 3.2 set POT5 1.0          # level jump (zipper test)
