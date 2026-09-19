# 125A MixEngine v1.0 Architecture

## Product variants

The project builds two VST3 classes from one shared DSP implementation.

### 125A MixEngine - Mix FX

A PreSonus-specific `Audio Mix Processor`. Studio One supplies the Mix FX processor with all participating channel buses. The plugin snapshots the current input block in `processMixControl()` and then lets each `processMixChannel(index)` read immutable snapshots of direct neighbours.

This makes Crosstalk:

- true channel-to-channel interaction
- direct-neighbour only
- stereo-lane preserving
- independent of channel callback order

The nominal maximum Crosstalk coefficient is 0.018 per direct neighbour.

### 125A MixEngine Channel

A normal VST3 audio effect using the same processing chain and GUI family. It has its own processor/controller UIDs and excludes the PreSonus Mix FX interfaces at compile time.

Parameter ID 7 is retained as a hidden legacy state slot for compatibility, but the standard Channel processor does not use Crosstalk.

## Shared signal path

`INPUT -> CONSOLE -> TUBE -> TAPE -> GLUE -> VINYL -> STEREO -> OUTPUT`

## Console

Console Drive uses a true zero-neutral nonlinear law. At 0%, the nonlinear transfer returns the input unchanged. Increasing Drive progressively morphs into the selected generic console curve.

The Console section also owns component tolerance, DC blocking, optional Console Noise and - in Mix FX only - inter-channel Crosstalk.

## Tube

Three generic voices map to progressively stronger drive/bias/asymmetry. Amount 0% is neutral.

## Tape

Studio reel-to-reel model with Amount, speed-dependent tone/low bump, transport Stability and independent Hiss.

## Glue

Linked stereo detector with fixed design ranges. Amount controls compression strength and blend; Response moves internal attack/release behaviour from slower/smoother toward faster/tighter.

## Vinyl

Color and Wear shape tone/body/saturation. Surface is independent and adds noise plus sparse click/pop events. Wear can therefore age the signal without mandatory crackle.

## Stereo

M/S stage:

- Width scales Side from 0% to 200%; 100% is neutral.
- Low Mono progressively removes low Side content with a fixed 120 Hz transition.
- Depth operates only on the upper Side band around 2 kHz and above, with approximately +/-4 dB endpoints.

## Metering and calibration

Reference choices are -18, -14 and -10 dBFS for 0 VU. VU detection uses an energy envelope with roughly 300 ms settling behaviour. Input metering is post Input Gain; Output metering is post full DSP and Output Gain.

Clip indicators trigger at digital full scale and hold for approximately 0.75 s.

## Quality and latency

Quality modes use 1x / 2x / 4x oversampling in relevant nonlinear islands.

The plugin reports a fixed host latency of **21 samples**. Internal paths with less bulk delay are aligned to the fixed target; Bypass is also delayed so host PDC does not change with module or Quality selection.

## Automation

Stored parameters are host-automatable. Processing consumes the latest parameter point received for the current audio block. No sample-accurate interpolation claim is made.

## GUI

The front panel is rendered procedurally with VSTGUI. The authoritative 125A logo is embedded from the vector master. Central layout JSON files generate and verify dedicated Mix FX and Channel UIDESC files.

The Channel UI omits Crosstalk. The Mix FX UI exposes it.
