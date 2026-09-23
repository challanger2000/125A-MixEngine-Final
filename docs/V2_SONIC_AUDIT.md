# 125A MixEngine V2 Sonic Audit

Branch: `v2.0.0-sonic-overhaul`

## Scope

The V2 work keeps the existing product concept, GUI layout, parameter IDs, fixed signal order and Mix FX / Channel architecture. The goal is to replace or substantially refine the sonic behaviour behind the existing controls.

Signal path:

`INPUT -> CONSOLE -> TUBE -> TAPE -> GLUE -> VINYL -> STEREO -> OUTPUT`

## Baseline findings

### Gain staging / REF / Level Match

- The current default calibration parameter is 0.5, which selects -14 dBFS = 0 VU.
- V2 target default for new instances: -18 dBFS = 0 VU.
- Input remains a real pre-chain drive control.
- Level Match must preserve drive into nonlinear stages while compensating predictable gain changes without becoming an adaptive loudness rider.
- Output remains an independent manual trim.

### Console

Current implementation:
- one low-band state around 180 Hz;
- four mode-dependent variants of one normalized tanh soft-clip family;
- very small static channel tolerance and bias;
- oversampling and DC blocking;
- independent colored noise.

V2 target:
- genuinely different mode transfer families, not only different coefficients;
- frequency-dependent nonlinear behaviour;
- slow dynamic bias / operating-point memory;
- level-dependent odd/even harmonic balance;
- restrained channel tolerances;
- crosstalk spectral shaping appropriate to a console path;
- Drive 0 remains identity for the nonlinear core.

### Tube

Current implementation:
- stateless tanh-based two-stage shaper;
- three voices mainly differ by drive, bias, asymmetry and second-stage amount;
- fixed parallel blend;
- no tube-state memory or frequency-dependent drive.

V2 target:
- stateful common-cathode-inspired behaviour;
- separate low/high drive behaviour;
- slow bias shift / cathode-memory approximation;
- level-dependent compression/sag;
- clearly distinct 12AU7 / 12AT7 / 12AX7 response families;
- controlled even/odd harmonic evolution with input level;
- oversampled nonlinear core;
- switched-on 0% Amount retains subtle base character.

### Tape

Current implementation:
- static magnetic-style soft curve `x/sqrt(1+k*x^2)`;
- fixed attack/release envelope compression;
- static speed-dependent low-pass and head-bump values;
- derivative-based wow/flutter approximation;
- independent hiss.

V2 target:
- hysteretic/history-dependent magnetic core rather than memoryless saturation;
- speed-dependent head-bump, HF loss and saturation behaviour as one coupled system;
- level-dependent HF loss;
- program-dependent compression;
- transport modulation with deterministic low-rate wow plus higher-rate flutter;
- independent hiss retained;
- robust bounded real-time implementation suitable for 1x/2x/4x quality modes.

### Glue

Current implementation:
- absolute-peak detector;
- one attack and one release time derived from Character;
- fixed 7 dB knee;
- ratio/threshold derived from Amount;
- fixed parallel-compression blend.

V2 target:
- fast/slow detector components;
- program-dependent dual release;
- crest-factor awareness;
- smoothed gain-control path;
- Character morphs detector/release/knee behaviour rather than merely increasing ratio;
- linked stereo remains phase-safe.

### Vinyl

Current implementation:
- static low-pass;
- low-frequency body boost;
- tanh shaping;
- colored surface noise;
- stochastic clicks whose rate and decay scale with Wear.

V2 target:
- Color affects spectral balance and tracing-like nonlinearity;
- Wear adds level/frequency-dependent HF degradation and transient softening;
- surface noise remains independent;
- clicks/pops become more statistically varied in level/shape while remaining bounded;
- no noise when Surface is zero.

### Stereo

Current implementation:
- conventional M/S width;
- 120 Hz low-side suppression;
- high-side +/-4 dB depth control above ~2 kHz.

V2 target:
- retain exact neutral point and mono compatibility;
- add slow correlation/energy awareness so extreme widening cannot unnecessarily worsen already side-heavy material;
- keep low-mono and depth behaviour predictable;
- no hidden Haas delay or inter-channel sample offset.

## Measurement requirements before release

Each colour module will be characterised at multiple control positions and input levels using:

1. static transfer curve;
2. small-signal gain;
3. THD and H2/H3/H4/H5 evolution;
4. harmonic growth versus input level;
5. frequency-dependent gain and distortion;
6. crest-factor / transient change;
7. attack/release or memory trajectory where applicable;
8. alias residual at 1x / 2x / 4x;
9. level-match error;
10. Channel/Mix FX numerical parity;
11. finite/bounded stress test;
12. fixed-latency and phase/mono verification.

## V2 rule

A module is not considered improved merely because it is more complex. The revised model must produce a measurable, musically useful response over the full parameter range and remain stable, bounded and host-safe.
