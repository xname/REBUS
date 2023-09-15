Fundamental methods of using REBUS with PD as they were documented during experimental development of a digital instrument patch.

In this directory, you will find patches demonstrating:
- [0-debug.pd](0-debug.pd)
  
  Basic input data post-processing workflow, which includes as smoothing and normalization, as well as debug printing/visualization compatible with BELA. 
- [1-basic.pd](1-basic.pd)
    
    Simple 1:1 mapping of input data to synthesis parameters to control unison pitch spread and other parameters.
- [2-derivative.pd](2-derivative.pd)

    Derivative mapping controlling a flange effect.
- [3-gesture.pd](3-gesture.pd)
    
    Simple gesture detection used for random walk modulation of the instrument state.

The instrument itself (with some modifications) is based on Stochastic Signal Density Modulation, which is a technique commonly used in hardware electronics.

Patches going further into that aspect topic can be found in [extras](extras).

___
[W K Werkowicz](https://github.com/wwerkk)

2023