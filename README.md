# 125A MixEngine

**125A MixEngine v1.1.0** is a Windows x64 VST3 coloration and summing processor with two editions built from the same DSP core:

- **125A MixEngine** - PreSonus Studio One **Mix FX** edition with true adjacent-channel console crosstalk.
- **125A MixEngine Channel** - standard **VST3 insert** edition for normal channel/bus use and VST3 hosts that do not expose the PreSonus Mix FX API.

## Signal flow

`INPUT -> CONSOLE -> TUBE -> TAPE -> GLUE -> VINYL -> STEREO -> OUTPUT`

The order is fixed by design. Console, Tube, Tape, Glue and Vinyl may be active at the same time.

## Main features

- Four generic Console characters: Clean, Classic, Vintage and Modern
- Corrected Console Drive law: 0% is neutral in the nonlinear core and saturation increases progressively
- True Mix FX adjacent-channel Crosstalk; no fake L/R stereo bleed
- Independent Console Noise, Tape Hiss and Vinyl Surface controls
- Tube section with Soft / Balanced / Hot voices
- Studio reel-to-reel Tape section with 7.5 / 15 / 30 ips, Stability and Hiss
- Linked Glue dynamics
- Vinyl Color, Wear and optional Surface noise/clicks
- M/S Stereo section with Depth, Width and fixed-120-Hz Low Mono
- Selectable 0 VU reference: -18 / -14 / -10 dBFS
- Input/Output VU source selection and clip hold indicators
- Eco 1x / Normal 2x / High 4x quality modes
- Fixed reported host latency: **21 samples**
- UI scaling: 75 / 100 / 125 / 150%
- 32-bit and 64-bit audio sample processing
- Mono and stereo support

## Documentation

- [Deutsche Bedienungsanleitung](docs/125A_MixEngine_Bedienungsanleitung_DE.pdf)
- [English User Manual](docs/125A_MixEngine_User_Manual_EN.pdf)
- [Building from source](docs/BUILDING.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Release notes](RELEASE_NOTES.md)

## Installation

Copy the required VST3 bundle into:

`C:\Program Files\Common Files\VST3`

Then rescan plugins in the DAW.

### Which edition should I use?

Use **125A MixEngine** in Studio One's Mix FX slot when you want channel-aware console behaviour. Crosstalk couples only direct neighbouring Mix FX channels and preserves stereo lanes.

Use **125A MixEngine Channel** as a normal insert on tracks or buses. It uses the same coloration core but intentionally omits inter-channel crosstalk because a normal VST3 insert cannot access neighbouring DAW channels.

Both editions use separate VST3 identities and can be installed side by side.

## Gain staging

0 VU is a reference point, not a level every musical signal must constantly hit. With the default reference, **0 VU = -14 dBFS**. Dynamic music can show a lower VU reading while short DAW peaks are substantially higher.

Level Match is a fixed parameter-dependent compensation system. It is not an adaptive loudness normalizer.

## Build

Requirements:

- Windows x64
- Visual Studio 2022 C++ toolchain
- CMake 3.25+
- Steinberg VST3 SDK
- Network access for the pinned HIIR dependency when not already cached

Set `VST3_SDK_ROOT` and build the two release targets:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVST3_SDK_ROOT="C:/path/to/vst3sdk" -DSMTG_CREATE_PLUGIN_LINK=0
cmake --build build --config Release --target 125A-MixEngine 125A-MixEngine-Channel
```

The GitHub Actions workflow checks out a pinned VST3 SDK revision and can run the full DSP/validator suite.

## Validation

The final source includes diagnostics for:

- oversampling roundtrip, aliasing and latency
- Tube/Tape/Vinyl nonlinear paths
- Console Drive zero-neutrality
- Console/Tube ordering
- VU metering and data exchange
- fixed processor latency
- mono/phase behaviour
- host/block/sample-format matrix
- Channel/Mix FX DSP parity
- Level Match
- automation stress
- true Mix FX adjacent-channel Crosstalk
- GUI/resource/parameter contracts
- Steinberg VST3 validator for the Channel edition

The current GUI is procedural VSTGUI; no raster faceplate or prerendered knob assets are required.

## Source layout

- `source/` - shared DSP, VST3 processor/controller and procedural GUI
- `resource/` - authoritative 125A vector logo plus generated layout/UIDESC contracts
- `tests/` - objective DSP and integration diagnostics
- `scripts/` - GUI and architecture contract verification
- `docs/` - manuals, architecture and build instructions
- `.github/workflows/` - Windows x64 build and validation workflow

## License

Copyright 2026 125A Audio Software. All rights reserved. This repository does not grant an open-source license. See [LICENSE.txt](LICENSE.txt).
