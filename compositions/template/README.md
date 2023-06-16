# REBUS

REBUS - Electromagnetic Interactions

https://xname.cc/rebus

this template abstracts the common code between compositions

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

or add `CPPFLAGS=-DMODE=1` to bela make options

important notes:

- mouse cursor control is very low resolution in both time and space

- character of movement is completely different

- provided for development purposes only

### desktop with JACK (audio in port)

```
g++ -o rebus render.cpp -DMODE=2 -ljack \
  -std=c++17 -Wall -Wextra -pedantic -Wno-unused-parameter
```

important notes:

- currently broken for all but absolutely trivial examples

- provided for experimental purposes only

## scope

for bela (rebus, gui): audio out x2, audio in x2, magnitude, phase

## audio recording

for bela (rebus, gui): 64MB/min 6ch wav (audio out x2, audio in x2, magnitude, phase)

filename is named after current localtime and composition name

## composition API

in `composition.h`, implement:

```
const char *COMPOSITION_name = "will-be-put-in-wav-filename";
struct COMPOSITION { /* stuff here */ };
static inline bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C) { /* stuff here */ }
static inline void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, float out[2], const float in[2], const float magnitude, const float phase) { /* stuff here */ }
static inline void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C) { /* stuff here */ }
```

## compositions

### untitled example

retriggered decaying complex oscillator

### I Spectral That Hand Motion

8 octave pitch shift turns movement into sound
