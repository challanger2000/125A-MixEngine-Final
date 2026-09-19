# Release Notes

## 125A MixEngine v1.0.0 - 2026-09-19

Initial commercial release candidate/final source baseline.

### Included

- PreSonus Studio One Mix FX edition
- Standard Windows x64 VST3 Channel edition
- shared modular DSP chain: Console, Tube, Tape, Glue, Vinyl, Stereo
- true adjacent-channel Mix FX Crosstalk
- independent Console Noise, Tape Hiss and Vinyl Surface
- three reference levels and VU source selection
- 1x / 2x / 4x quality modes
- fixed 21-sample reported latency
- procedural VSTGUI with 75/100/125/150% UI scaling
- German and English PDF manuals

### Final Console Drive correction

Console Drive was recalibrated before release so that **0% is a true identity point in the nonlinear core**. Saturation, peak rounding and density now increase progressively with Drive instead of being strongly present at the zero position.

A dedicated diagnostic permanently guards this behaviour.

### Validation

The final validation suite covers DSP finiteness, oversampling, alias behaviour, latency, metering, mono/phase behaviour, Channel/Mix FX parity, Level Match, automation stress, real adjacent-channel Crosstalk, GUI contracts and the Steinberg validator for the Channel edition.
