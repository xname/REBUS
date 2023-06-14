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
g++ -o rebus render.cpp -DMODE=2 -ljack \
  -std=c++17 -Wall -Wextra -pedantic -Wno-unused-parameter
```

## scope

for bela (rebus, gui): audio out x2, audio in x2, magnitude, phase

## audio recording

for bela (rebus, gui): 64MB/min 6ch wav (audio out x2, audio in x2, magnitude, phase)

filename is named after current timestamp in hexadecimal
(todo: use a proper date format)

## composition API

in `composition.h`, implement:

```
struct COMPOSITION { /* stuff here */ };
static inline bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C) { /* stuff here */ }
static inline void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, float out[2], const float in[2], const float magnitude, const float phase) { /* stuff here */ }
static inline void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C) { /* stuff here */ }
```
