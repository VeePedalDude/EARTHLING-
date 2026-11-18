# VeesCreations

Finished builds by VeePedalDude (veepedaldude@gmail.com). Each one is a working
effect for the PedalPCB Terrarium (Daisy Seed, Arduino IDE + DaisyDuino).

## Layout

Give each build its own folder, named exactly like its sketch, so the Arduino
IDE can open it directly:

```
VeesCreations/
  MyFuzz/
    MyFuzz.ino     <- open this in the Arduino IDE
    MyFuzz.txt     <- same code as plain text, readable anywhere
```

## Test a build before adding it

```
./daisyrig test VeesCreations/MyFuzz --board terrarium --hw
```
