# rebus composition template

## platforms

### bela with rebus (antennas)

default mode, configured by

```
#define MODE MODE_REBUS
```

### bela with gui (mouse cursor)

change

```
#define MODE MODE_REBUS
```

to

```
#define MODE MODE_BELA
```

or add `-DMODE=1` to build options

### desktop with JACK (audio in port)

```
gcc -o rebus render.cpp -DMODE=2 -ljack \
  -std=c++17 -Wall -Wextra -pedantic -Wno-unused-parameter
```

## audio recording

for bela (rebus, gui): 45MB/min 4ch wav (audio out, controls)
