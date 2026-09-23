# Release Notes

## 125A MixEngine V2 v2.0.0 - 2026-09-24

### DSP / musical behavior

- Rebuilt from the final v1.1.0 codebase rather than the older development repository.
- 0% is a true neutral point for intensity controls.
- Amount/Drive laws are continuous and monotonic with no hard upper-range knee.
- Tube Voice and Tape Speed use continuous internal morphing.
- Tape keeps the v1.1.0 program-dependent compression envelope and adds bounded magnetic/hysteresis memory.
- Glue was reworked around a calibrated threshold, program-dependent release, amount-dependent attack, parallel-style blend and revised Level Match.
- Vinyl Color and Wear were separated into materially different sonic axes.
- Console mode identity was strengthened; Vintage adds bounded decaying transformer memory.

### Automation / host behavior

- Standard VST3 automation is applied at exact host-supplied sample offsets.
- Mix FX automation is also applied sample-accurately.
- Character transitions are smoothed to prevent hard switching artifacts.
- Fixed reported host latency remains 21 samples.
- Scoped FTZ/DAZ denormal protection is active during audio callbacks and the host MXCSR state is restored afterward.

### Validation

- Steinberg VST3 Validator: 47 tests passed, 0 failed on the validated Channel build.
- Channel/Mix FX sample-accuracy diagnostics: maxDiff = 0 across the tested automation matrix.
- Sample-rate/sample-format matrix passed at 44.1 / 48 / 96 / 192 kHz in float32 and float64 paths.
- Zero-neutrality, range, Tape memory, Glue transient/recovery, Vinyl material-axis, Console character/memory, quality switching, oversampling/alias, latency, phase/mono and Level Match diagnostics passed.
- Dedicated denormal/MXCSR restoration diagnostic added for V2.

## 125A MixEngine v1.1.0 - 2026-09-22

Previous production release and the final base from which V2 was rebuilt.
