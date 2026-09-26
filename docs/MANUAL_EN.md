# 125A MixEngine - User Manual

Version 3.0.0 - Windows x64 VST3

## 1. Overview

125A MixEngine is a modular coloration and summing processor delivered in two editions: **125A MixEngine V3** as a PreSonus Studio One Mix FX and **125A MixEngine V3 Channel** as a normal VST3 insert. Both use the same central DSP core. Only the Mix FX edition can access neighbouring DAW channels and therefore provide true channel-to-channel crosstalk.

Signal flow: **INPUT -> CONSOLE -> TUBE -> TAPE -> GLUE -> VINYL -> STEREO -> OUTPUT**.

0 VU is a reference point, not a level that every musical signal must constantly hit. With dynamic material, the VU reading can be considerably lower than the DAW peak meter.

## 2. Installation and editions

- **125A-MixEngine-V3.vst3**: PreSonus Studio One Mix FX. Load it in the Mix FX slot of a bus/main section.
- **125A-MixEngine-V3-Channel.vst3**: standard VST3 insert for tracks, buses and other VST3 hosts.
- Windows installation: copy the VST3 bundle to `C:\Program Files\Common Files\VST3` and rescan plugins in the host.
- Both editions use separate plugin IDs and may be installed side by side.

## 3. Metering, Reference Level and gain staging

**Reference Level** defines which digital level corresponds to 0 VU: -18, -14 or -10 dBFS. The default is -14 dBFS.

The VU meter uses an energy envelope with roughly 300 ms settling behaviour. It is not a peak meter. The CLIP indicator triggers at peak >= 0 dBFS and holds for roughly 0.75 s.

**VU Source Input** is measured after Input Gain. **VU Source Output** is measured after the complete DSP including Output Gain.

**Level Match** is fixed parameter-dependent compensation. It is not an adaptive loudness normalizer.

## 4. Complete control reference

| Control | Type | Range | Default | Function |
|---|---|---|---|---|
| Bypass | Switch | Active / Bypass | Active | Bypasses coloration while preserving fixed host latency. |
| Input Gain | Knob | -12.0 to +12.0 dB | 0.0 dB | Level before all coloration stages. |
| Reference Level | Selector | -18 / -14 / -10 dBFS | -14 dBFS | Defines 0 VU and internal calibration. |
| Level Match | Switch | Off / On | On | Fixed compensation for coloration stages. |
| Console On | Switch | Off / On | On | Enables Console. |
| Console Mode | Selector | Clean / Classic / Vintage / Modern | Classic | Four generic console transfer characters. |
| Console Drive | Knob | 0 to 100 % | 25 % | 0 % is neutral in the nonlinear core; higher Drive adds saturation, peak rounding and density. |
| Crosstalk | Knob, Mix FX only | 0 to 100 % | 10 % | True bleed to direct neighbour channels; max nominal about 1.8 % (-34.9 dB) per neighbour. |
| Console Noise | Knob | 0 to 100 % | 0 % | Independent coloured console floor noise. |
| Tube On | Switch | Off / On | Off | Enables Tube. |
| Tube Voice | Selector | Soft / Balanced / Hot | Balanced | Selects progressively stronger drive/bias/asymmetry behaviour. |
| Tube Amount | Knob | 0 to 100 % | 20 % | Tube saturation amount; 0 % is neutral. |
| Tape On | Switch | Off / On | Off | Enables studio tape. |
| Tape Amount | Knob | 0 to 100 % | 20 % | Tape saturation, soft compression and wet contribution. |
| Tape Speed | Selector | 7.5 / 15 / 30 ips | 15 ips | Changes bandwidth, low bump and transport character. |
| Tape Stability | Knob | 0 to 100 % | 90 % | 100 % = stable; lower values increase wow/flutter. |
| Tape Hiss | Knob | 0 to 100 % | 0 % | Independent tape-coloured hiss. |
| Glue On | Switch | Off / On | Off | Enables Glue. |
| Glue Amount | Knob | 0 to 100 % | 15 % | Increases compression strength and blend. |
| Glue Response | Knob | 0 to 100 % | 50 % | Low = slower/smoother; high = faster/tighter. |
| Vinyl On | Switch | Off / On | Off | Enables Vinyl. |
| Vinyl Color | Knob | 0 to 100 % | 25 % | Base character: softer top end, body and saturation. |
| Vinyl Wear | Knob | 0 to 100 % | 0 % | Age/use: darker top end plus additional coloration and saturation. |
| Vinyl Surface | Knob | 0 to 100 % | 0 % | Surface noise plus occasional clicks/pops. |
| Depth | Knob | -100 to +100 % | 0 % | Processes upper Side around 2 kHz and above; endpoints about +/-4 dB. |
| Width | Knob | 0 to 200 % | 100 % | M/S width; 100 % is neutral. |
| Low Mono | Knob | 0 to 100 % | 0 % | Progressively removes low Side with a fixed 120 Hz transition. |
| Output Gain | Knob | -12.0 to +12.0 dB | 0.0 dB | Final level after all stages. |
| VU Source | Selector | Input / Output | Output | Chooses meter position. |
| Quality | Selector | Eco 1x / Normal 2x / High 4x | Normal 2x | Oversampling factor for relevant nonlinear stages. |
| UI Scale | Button | 75 / 100 / 125 / 150 % | 100 % | Each click cycles to the next GUI size. |
| VU L/R | Meter | -20 to +3 VU | - | Energy-based VU display. |
| CLIP L/R | Indicator | off / on | - | Peak >= 0 dBFS, hold about 0.75 s. |

## 5. Console in detail

**Drive 0 %** is a true identity point in the nonlinear Console core. An enabled Console may still contain fixed properties such as channel tolerances, DC blocking, optional noise and - in Mix FX - Crosstalk. The selected mode's actual saturation increases with Drive.

- **Clean**: most restrained transfer.
- **Classic**: more density, slight low-frequency coupling and a small high-frequency/asymmetric component.
- **Vintage**: strongest low/asymmetric coloration and highest saturation tendency.
- **Modern**: more high-frequency-oriented shaping with a comparatively tight feel.

Crosstalk couples only **direct neighbours**. Channel 1 couples to channel 2, not directly to channel 3. A middle channel can couple to both direct neighbours. Coupling is lane-preserving: left stays left and right stays right. The standard Channel edition deliberately has no Crosstalk control.

## 6. Tube

Soft is the mildest voice, Balanced is the middle setting and Hot is the strongest. Amount 0 % is neutral. Increasing Amount raises drive, harmonic density and asymmetry.

## 7. Tape

Amount controls saturation and soft compression. **7.5 ips** rolls the top end off earlier and has the strongest low bump, **15 ips** is balanced, and **30 ips** is more extended and clean. Stability 100 % means stable transport; lower values increase wow/flutter. Hiss is independent and can remain at 0.

## 8. Glue

Glue is not a fully parameterised bus compressor. Amount increases the compression effect. Response moves internal attack/release behaviour from slower/smoother toward faster/tighter. Stereo detection is linked.

## 9. Vinyl

**Color** sets the base character. **Wear** models age/use and can darken and saturate the signal without mandatory crackle. **Surface** is the actual surface layer with noise and click/pop events; Wear also affects their character when Surface is active.

## 10. Stereo

Width is conventional M/S width control. 100 % is neutral.

Depth is deliberately not a second Width control. It only affects the upper Side region around 2 kHz and above. +100 % attenuates that region by about 4 dB; -100 % boosts it by about 4 dB.

Low Mono uses a fixed 120 Hz transition and progressively removes low-frequency Side content. 100 % is full Low Mono action, not a brickwall crossover.

## 11. Quality, latency and automation

Eco = 1x, Normal = 2x, High = 4x oversampling in relevant nonlinear stages.

For a given sample rate, the plugin reports fixed host latency independent of Quality, module state and Bypass: **28 samples at 44.1 kHz, 29 at 48 kHz, 36 at 96 kHz and 50 at 192 kHz**.

Parameters are host-automatable. Both the standard VST3 and Mix FX paths apply parameter changes at the sample offsets supplied by the host; dedicated diagnostics verify sample-accurate automation behaviour.

## 12. Technical data

- Format: VST3, Windows x64
- Editions: PreSonus Studio One Mix FX + Standard VST3 Channel
- Audio: mono/stereo, 32-bit and 64-bit sample processing
- Fixed host latency per sample rate: 28 samples @44.1 kHz / 29 @48 kHz / 36 @96 kHz / 50 @192 kHz
- Oversampling: 1x / 2x / 4x
- 0 VU: -18 / -14 / -10 dBFS
- VU response: about 300 ms
- Clip hold: about 0.75 s
- Low Mono: fixed 120 Hz transition
- Depth: upper Side split around 2 kHz, endpoints about +/-4 dB
- Mix FX Crosstalk: direct neighbours, nominal max coefficient 0.018

## 13. Mastering note

125A MixEngine is a coloration processor, not a limiter or complete mastering system. Final loudness, true-peak limiting and platform targets are set with separate mastering tools.

Copyright 2026 125A Audio Software. All rights reserved.
