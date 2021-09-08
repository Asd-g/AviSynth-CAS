## Description

[Contrast Adaptive Sharpening.](https://gpuopen.com/fidelityfx-cas/)

This is [a port of the VapourSynth plugin CAS](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-CAS).

### Requirements:

- AviSynth 2.60 / AviSynth+ 3.4 or later

- Microsoft VisualC++ Redistributable Package 2022 (can be downloaded from [here](https://github.com/abbodi1406/vcredist/releases))

### Usage:

```
CAS (clip, float "sharpness", int "y", int "u", int "v", int "opt")
```

### Parameters:

- clip\
    A clip to process. All planar formats are supported.
    
- sharpness\
    Sharpening strength.\
    Must be between 0.0 and 1.0.\
    Default: 0.5.
    
- y, u, v\
    Planes to process.\
    1: Return garbage.\
    2: Copy plane.\
    3: Process plane. Always process planes when the clip is RGB.\
    Default: y = 3; u = v = 2.
    
- opt\
    Sets which cpu optimizations to use.\
    -1: Auto-detect.\
    0: Use C++ code.\
    1: Use SSE2 code.\
    2: Use AVX2 code.\
    3: Use AVX512 code.\
    Default: -1.