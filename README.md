# MicSmith

A microphone-modelling plugin for Windows (VST3 + Standalone), in the spirit of
IK Multimedia's T-RackS *Mic Room*.

You tell it which microphone a take was actually recorded with, and which
microphone you want it to sound like. MicSmith computes the transfer function
that takes you from one to the other and applies it as a cascade of
minimum-phase filters, with the physical controls you would normally reach for
next: proximity, off-axis angle, low/high cut and mix.

## Signal path

```
input gain
  -> target microphone colouration
  -> exact inverse of the source microphone
  -> proximity (low-shelf + low-mid build-up)
  -> off-axis (high-shelf loss + upper-mid notch)
  -> low cut (12 dB/oct) and high cut (12 dB/oct)
  -> dry/wet mix
  -> auto gain + output gain
  -> phase invert
```

### Why the maths is simple and stable

Each microphone is a handful of RBJ peak/shelf bands. Those coefficients are
exactly symmetric under `A -> 1/A`, so a band with the gain negated is the
*exact* and *always-stable* inverse of the same band. That means:

* the source->target correction is just "target bands, then source bands
  inverted" - no deconvolution, no FFT, no stored impulse responses;
* setting the source microphone equal to the target gives a bit-exact null
  (`tools/dsp_test.cpp` asserts this is below 1e-9 dB at 44.1/48/96 kHz);
* every filter is guaranteed stable, which the same test also asserts;
* the whole thing is ~16 biquads per channel at worst, so it costs almost
  nothing on the CPU.

Every response is also level-matched with an auto-gain over the music band, so
auditioning models does not jump in volume.

## Controls

| Control | What it does |
| --- | --- |
| **Recorded With** | Source microphone - the one the take actually used. |
| **Emulate** | Target microphone - the one you want it to sound like. |
| **Input / Output** | +/-24 dB of gain staging. |
| **Proximity** | Pressure build-up from working close to the capsule. |
| **Off-Axis** | Top-end loss and upper-mid notch as the source moves off-axis. |
| **Low Cut / High Cut** | Clean 12 dB/oct filters, after the model. |
| **Mix** | Parallel blend of the modelled and the dry signal. |
| **Phase** | Output polarity invert. |
| **A / B / COPY** | Two comparison slots; COPY pushes the current settings to the other slot. |
| **Factory Presets** | Practical starting points (phone/laptop rescue, vocal chains, guitar cab, kick layering...). |

The graph shows three curves: the **source** microphone (blue dashed), the
**target** microphone (amber dashed) and the resulting **correction** (solid
amber, auto-gain applied).

## Microphone models

19 models across the families you would expect: large- and small-diaphragm
condensers (warm, modern, vintage tube, natural, bright), moving-coil dynamics
(studio, broadcast, instrument, handheld, kick), ribbons (vintage, modern,
passive/dark), sub-kick, lavalier/headset, shotgun, and consumer devices
(smartphone, laptop built-in) which are the most useful *sources* in practice.

These are original, physically motivated approximations of each family's broad
characteristics - not measured IRs and not copies of any commercial product's
data. No trademarked microphone names are used.

## Building

Requirements: Visual Studio 2022 with the C++ workload (present on this
machine), and CMake. The repo builds against a vendored JUCE 8.0.6 checkout in
`JUCE/`.

```bat
build.bat
```

or manually:

```bat
tools\cmake\bin\cmake -S . -B build -G "Visual Studio 17 2022" -A x64
tools\cmake\bin\cmake --build build --config Release --parallel
```

Outputs:

```
build\MicSmith_artefacts\Release\VST3\MicSmith.vst3
build\MicSmith_artefacts\Release\Standalone\MicSmith.exe
```

### Installing the VST3

Copy the `MicSmith.vst3` **folder** into either:

* `C:\Program Files\Common Files\VST3` (all users, needs admin), or
* your DAW's user VST3 folder.

Then rescan plugins in your DAW.

### macOS

There is **no prebuilt macOS binary in this folder** - a macOS plug-in is a
Mach-O bundle and cannot be produced on Windows. The project is set up to build
on a Mac, including macOS 10.13 High Sierra:

```bash
./build_macos.sh      # Audio Unit + VST3 + Standalone, targeting 10.13
./install_macos.sh    # installs into ~/Library/Audio/Plug-Ins
```

See [BUILD-MACOS.md](BUILD-MACOS.md) for the walkthrough, the Audio Unit format
needed by Logic and GarageBand, and the `libc++` caveat that can stop a plug-in
built with a recent Xcode from loading on 10.13-10.15.

## Tests

Two test programs, both JUCE-free-of-the-DAW:

**DSP maths** - built by default.

```bat
build\Release\MicSmithDSPTest.exe
```

Checks that the source==target correction is an exact null, that every biquad
of every pair at extreme settings is stable, that the output stays finite under
noise and that impulse responses decay (no self-oscillation), plus a response
report card for every model.

**Host load test** - loads a built/installed bundle through JUCE's VST3 host
implementation, the same code path a DAW uses. Excluded from the default build:

```bat
tools\cmake\bin\cmake --build build --config Release --target MicSmithHostTest
build\MicSmithHostTest_artefacts\Release\MicSmithHostTest.exe "C:\Program Files\Common Files\VST3\MicSmith.vst3"
```

Verifies the host discovers and instantiates the plugin, sees all 11
parameters, passes audio through it, hears a change when the model is switched,
and can serialise state.

## Licence

The plugin source links JUCE, which is dual-licensed (GPLv3 or a commercial
JUCE licence). Building and distributing this plugin means complying with one
of those. The MicSmith source itself is yours to use.
