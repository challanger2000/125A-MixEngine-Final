# Building 125A MixEngine V3 v3.0.0

## Supported final build

The release configuration targets **Windows x64 VST3** and produces two plugins from the same source tree:

- `125A-MixEngine-V3.vst3` - PreSonus Mix FX
- `125A-MixEngine-V3-Channel.vst3` - standard VST3 audio effect

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 with Desktop development with C++
- CMake 3.25 or newer
- Steinberg VST3 SDK
- Git/network access for CMake FetchContent when HIIR is not cached

The CI workflow pins the Steinberg SDK to commit:

`3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`

The HIIR dependency is pinned to:

`4589fedb4d08b899514cb605ccd7418bf262ab18`

## Configure

PowerShell example:

```powershell
$env:VST3_SDK_ROOT = "C:\dev\vst3sdk"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVST3_SDK_ROOT="$env:VST3_SDK_ROOT" -DSMTG_CREATE_PLUGIN_LINK=0
```

## Build both final editions

```powershell
cmake --build build --config Release --target 125A-MixEngine-V3 125A-MixEngine-V3-Channel
```

## Diagnostics

The repository defines standalone diagnostic targets. The GitHub Actions full-validation workflow builds and runs all of them, then builds Steinberg's validator and validates the Channel VST3.

Important contracts include:

- Console Drive 0% must be identity in the nonlinear core.
- Reported host latency is fixed for a given sample rate and independent of Quality/modules/Bypass (29 samples at 48 kHz).
- Mix FX and Channel share the same DSP core.
- Standard Channel processing must not use the Mix FX Crosstalk parameter.
- Mix FX Crosstalk is direct-neighbour, lane-preserving and callback-order independent under the observed Studio One Mix FX scheduling model.
- The GUI is generated from central layout contracts and uses procedural VSTGUI controls.
