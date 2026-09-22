# Release Notes

## 125A MixEngine v1.1.0 - 2026-09-22

### Tube / Tape refinement

- Tube and Tape now retain a subtle hardware-like base character when the module is enabled at 0% Amount.
- Amount controls now span from subtle coloration to substantially stronger character at 100%.
- Tube and Tape Level Match compensation was recalibrated against the refined DSP.
- Input Level Match now compensates the linear input-gain component while preserving nonlinear drive into the processing chain.
- Tape processing retains independent Hiss control and correct oversampling/latency participation at 0% Amount.
- Channel and Mix FX editions use the same updated DSP core.

### Validation

- Steinberg Validator: 47/47 tests passed.
- Full DSP diagnostics passed, including Tube/Tape oversampling, aliasing, fixed 21-sample latency, phase/mono behaviour, Level Match, automation stress, and exact Channel/Mix FX DSP parity.
- Measured Level Match at the production test point: Tube +0.0013 dB, Tape -0.0049 dB, full chain -0.0685 dB.

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
