# 125A MixEngine V2

**125A MixEngine V2 v2.0.0** is a Windows x64 VST3 coloration, summing and mix-finishing processor with two editions built from the same DSP core:

- **125A MixEngine V2** - PreSonus Studio One **Mix FX** edition with true adjacent-channel console crosstalk.
- **125A MixEngine V2 Channel** - standard **VST3 insert** edition for tracks, buses and VST3 hosts without the PreSonus Mix FX API.

## Signal flow

`INPUT -> CONSOLE -> TUBE -> TAPE -> GLUE -> VINYL -> STEREO -> OUTPUT`

## V2 highlights

- Sample-offset accurate automation in both standard VST3 and Mix FX paths
- True 0% neutral intensity points
- Continuous monotonic amount/drive laws: 20-50% musical working range, 75-100% strong/creative
- Continuous Tube Voice and Tape Speed morphing
- Tape: retained v1.1 program-dependent compression plus V2 magnetic/hysteresis memory
- Glue: program-dependent release and improved transient behavior
- Vinyl: clearly separated Color and Wear axes
- Console: stronger character separation and bounded Vintage transformer memory
- Scoped FTZ/DAZ denormal protection inside audio callbacks with host MXCSR restoration
- Eco 1x / Normal 2x / High 4x quality modes
- Fixed reported host latency: **21 samples**
- UI scaling: 75 / 100 / 125 / 150%
- 32-bit and 64-bit audio processing
- Mono and stereo support
- Tested at 44.1 / 48 / 96 / 192 kHz

## Installation

Copy the desired VST3 bundle to:

`C:\Program Files\Common Files\VST3`

Then rescan VST3 plugins in the DAW.

### Which edition?

Use **125A MixEngine V2** in Studio One's Mix FX slot for channel-aware console behavior and true adjacent-channel crosstalk.

Use **125A MixEngine V2 Channel** as a normal insert on tracks or buses.

## Gain staging

0 VU is a reference point, not a target every signal must constantly hit. The selectable references are -18 / -14 / -10 dBFS.

Level Match is parameter-dependent compensation, not an adaptive loudness normalizer. Input Level Match compensates the linear input-gain component while preserving changed drive into nonlinear stages.

## Validation

The V2 source includes diagnostics for zero-neutrality, range continuity, oversampling/aliasing, fixed latency, metering, mono/phase behavior, Channel/Mix FX parity, Level Match, automation stress, sample-offset accuracy, real Mix FX crosstalk, character morphing, Tape/Console memory, Glue transients, Vinyl axis separation, quality switching, sample-rate/sample-format matrix, denormal hardening and the Steinberg VST3 validator.

## Documentation

The Gumroad package contains complete German and English V2 PDF manuals.

## License

Copyright 2026 125A Audio Software. All rights reserved. See [LICENSE.txt](LICENSE.txt).
