# Release Notes

## 125A MixEngine V3 v3.0.0 - 2026-09-26

### DSP / sound engine

- New V3 Console, Tube, Tape and Vinyl processing with higher-detail nonlinear modelling.
- Preserves the established MixEngine signal flow:
  `INPUT -> CONSOLE -> TUBE -> TAPE -> GLUE -> VINYL -> STEREO -> OUTPUT`.
- True 0% neutral points remain enforced for intensity controls.
- Quality modes remain Eco 1x / Normal 2x / High 4x.
- Mix FX Crosstalk remains true adjacent-channel, lane-preserving and callback-order independent.
- Level Match remains fixed, parameter-dependent compensation rather than an adaptive loudness normalizer.
- VU reference calibration is corrected so a DAW sine set to -18 / -14 / -10 dBFS lands at 0 VU for the selected reference.

### Host / automation / stability

- Standard VST3 and Mix FX automation are applied at host-supplied sample offsets.
- Fixed host latency per sample rate, independent of module state, Quality and Bypass:
  - 44.1 kHz: 28 samples
  - 48 kHz: 29 samples
  - 96 kHz: 36 samples
  - 192 kHz: 50 samples
- Complete V3 state roundtrip and V2/intermediate migration matrix verified.
- Offline/realtime parity, lifecycle/restart, non-finite recovery and denormal safety verified.
- Mono/stereo, float32/float64 and sample-rate matrices verified.

### GUI

- New 125A hardware-style V3 interface with raised blue module plates on a dark chassis.
- High-resolution Vernier and gunmetal controls.
- Restored 125A VU artwork with calibrated live needle.
- Integrated CLIP jewel indicators.
- 75 / 100 / 125 / 150% UI scaling.
- Final layout and asset geometry guarded by automated QA.

### Validation

- Steinberg VST3 Validator: 47 tests passed, 0 failed on the standard Channel build.
- Full V3 Research Diagnostics passed.
- Channel/Mix FX latency and DSP parity diagnostics passed.
- Sample-accurate automation, crosstalk, metering, auto-level, CPU scaling, aliasing, state, phase/mono, program-material and robustness diagnostics passed.

## 125A MixEngine V2 v2.0.0 - 2026-09-24

Previous production generation.
