# K Channel

A 9000K-style console channel strip for Windows and macOS (VST3 + Standalone),
with Quell's dynamic resonance suppressor built into the signal path. A legacy
VST2 build is included for Windows 7 and Cubase 5.

This is an original implementation built on JUCE. It is inspired by the layout
and working practice of a large-format SSL 9000K channel, but it is not a
circuit clone, does not use any proprietary data, and is not affiliated with
SSL. The EQ, filters, gate, compressor and Quell engine are all original DSP.

## Signal path

```
input trim / polarity
  -> high-pass and low-pass filters
  -> four-band equalizer (LF shelf, LM bell, HM bell, HF shelf)
  -> QUELL (dynamic resonance suppression)
  -> gate
  -> RMS compressor
  -> output trim
```

**Quell is before the compressor by default.** That is the more useful place
for it on a console channel: narrow resonances, harsh vocal rings and cymbal
spikes are removed before they reach the compressor detector, so gain reduction
follows the actual level of the source instead of clamping down on a single
resonant peak. The compressor then works on the cleaned signal and its make-up
gain cannot re-emphasise a resonance that Quell would otherwise have caught.

The **Q POST** button moves Quell after the compressor. Use it when the
compressor's own density or make-up gain is what makes a resonance objectionable,
or when you want the compressor to react to the raw channel and Quell to clean up
the result. Quell keeps the same one-frame latency in either position, so the
host's delay compensation stays valid.

## Controls

### Input / filters

| Control | Range | What it does |
| --- | --- | --- |
| **Input** | -24..+24 dB | Trim before the channel |
| **HPF** | 20..500 Hz | 12 dB/oct high-pass |
| **LPF** | 3..20 kHz | 12 dB/oct low-pass |
| **FILTERS** | on/off | Engages both filters |
| **PHASE** | on/off | Inverts polarity |

### Equalizer

Four bands, all with ±15 dB gain:

* **LF** — low shelf, 30–450 Hz
* **LM** — low-mid bell, 80–1500 Hz, variable Q
* **HM** — high-mid bell, 1–12 kHz, variable Q
* **HF** — high shelf, 2.5–18 kHz

The **EQ** button bypasses the whole four-band section. The display shows the
combined filter/EQ response.

### Dynamics

The gate comes before the compressor. Both use linked stereo detection.

**Compressor:** threshold -48..0 dB, ratio 1:1–20:1, attack 0.1–100 ms,
release 20–2000 ms, make-up 0–24 dB, 6 dB soft knee. It is a feed-forward RMS
compressor with a fast attack and a longer release, in the spirit of a console
channel compressor.

**Gate:** threshold -70..0 dB, range 0–60 dB, attack 0.1–50 ms, release
20–1000 ms. It has 2 dB of hysteresis once open so it does not chatter around
the threshold.

### Quell

Quell is the same spectral resonance suppressor used by the standalone Quell
plug-in, with its full control set:

| Control | What it does |
| --- | --- |
| **Depth** | How much detected excess is removed |
| **Sensitivity** | How far a peak must rise above the local spectrum before it is touched |
| **Selectivity** | How tightly the detector focuses on narrow rings |
| **Speed** | Attack/release of the suppression curve |
| **Low Tame** | Backs the processing off below roughly 700 Hz |
| **High Tame** | Backs it off above roughly 5 kHz |

**QUELL** bypasses the suppressor while preserving its latency. **Q POST**
moves it after the compressor.

### Master

**Output** is the final trim (-24..+24 dB). **BYPASS** is a hard bypass that
still runs the Quell delay line, so the reported latency does not change.

The editor opens at 92% of the available screen area and preserves its 5:3
aspect ratio. Drag the bottom-right corner to resize it; the minimum is
900x540, so every control remains reachable on a laptop display.

## Latency

Quell uses one FFT frame of latency:

| Sample rate | FFT size | Latency |
| --- | --- | --- |
| 44.1 / 48 kHz | 1024 | 21.3 ms at 48 kHz |
| 88.2 / 96 kHz | 2048 | 21.3 ms at 96 kHz |

The same latency is reported whether Quell is before or after the compressor,
and whether it is enabled or bypassed.

## Building (Windows)

The repository vendors JUCE at `../JUCE` and CMake at `../tools/cmake`.

```bat
KChannel\build.bat
```

Outputs:

```text
KChannel\build\KChannel_artefacts\Release\VST3\K Channel.vst3
KChannel\build\KChannel_artefacts\Release\Standalone\K Channel.exe
```

Copy the `K Channel.vst3` **folder** into your user VST3 folder or
`C:\Program Files\Common Files\VST3`, then rescan in the DAW.

### Windows 7 / Cubase 5 VST2

Cubase 5 loads VST2, so the project can also produce a 32-bit and 64-bit VST2
DLL. The current release ZIP contains both:

```text
KChannel-VST2-Windows7.zip
  VST2-32bit/K Channel.dll   <- use this with 32-bit Cubase 5
  VST2-64bit/K Channel.dll   <- use this with 64-bit Cubase 5
```

Copy the DLL into the VST2 folder Cubase scans, then restart Cubase or use
**Devices > Plug-in Information > Update**. A 32-bit Cubase cannot load the
64-bit DLL, and a 32-bit Windows installation can only use the 32-bit DLL.

The VST2 build uses JUCE 7.0.12 because JUCE 8's minimum deployment target is
Windows 10. It is compiled with `WINVER`/`_WIN32_WINNT` set to Windows 7, the
static MSVC runtime, and the legacy VST2 wrapper. The 32-bit DLL is statically
linked and imports only Windows 7-era system DLLs.

To rebuild the legacy binaries, place JUCE 7.0.12 in `..\JUCE7` and the VST2 SDK
headers under `third_party\vst2sdk\pluginterfaces\vst2.x\`, then run:

```bat
KChannel\build-vst2-win7.bat
```

VST2 is legacy Steinberg technology. Building and distributing VST2 plug-ins
requires the appropriate Steinberg VST2 licence; the SDK headers are not
included in this repository.

## Building (macOS)

A macOS VST3 is a Mach-O bundle and must be built on macOS with Xcode; it
cannot be produced by the Windows build. The project is configured for macOS
10.13 High Sierra and x86_64 by default, and the same binary runs on newer Intel
Macs and on Apple Silicon through Rosetta.

On a Mac with Xcode/Command Line Tools, CMake and JUCE 8.0.6:

```bash
./KChannel/build-macos.sh
```

That builds only the VST3 and writes:

```text
KChannel/build-mac/KChannel_artefacts/Release/VST3/K Channel.vst3
KChannel/dist/KChannel-VST3-macOS10.13-x86_64.zip
```

The script sets `CMAKE_OSX_DEPLOYMENT_TARGET=10.13` and
`CMAKE_OSX_ARCHITECTURES=x86_64`. If the plug-in builds with a recent Xcode but
fails to load on High Sierra through Catalina, rebuild with the old libc++
linking mode:

```bash
KCHANNEL_LD_CLASSIC=ON ./KChannel/build-macos.sh
```

`.github/workflows/build-kchannel-macos.yml` performs the same build on an
Intel macOS runner and uploads the ZIP as a workflow artifact. It also checks
that the binary reports a 10.13 deployment target and runs the DSP and VST3
host tests.

### Download the macOS VST3

After a release has been published, the latest macOS package is available at:

https://github.com/mylatinflutesmusic/micsmith/releases/latest/download/KChannel-VST3-macOS10.13-x86_64.zip

The release workflow builds and tests the same VST3, then attaches the ZIP to
the GitHub release. GitHub requires you to be signed in if the repository is
private.

Copy the unzipped `K Channel.vst3` **bundle** into
`~/Library/Audio/Plug-Ins/VST3/` (or `/Library/Audio/Plug-Ins/VST3/` for all
users), then rescan in the DAW. If it was downloaded from another Mac, remove
the quarantine flag and ad-hoc sign it:

```bash
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/K Channel.vst3"
codesign --force --deep --sign - "$HOME/Library/Audio/Plug-Ins/VST3/K Channel.vst3"
```

VST3 is for modern DAWs. Cubase 5 only loads VST2, so the Windows
`KChannel-VST2-Windows7.zip` package is the relevant one for that version.

## Tests

```bat
KChannel\test.bat
```

The JUCE-free DSP test checks the peaking/shelf/filter responses, compressor
threshold and gain-reduction behavior, gate attenuation and 96 kHz stability.

After building the VST3, the host test loads the actual bundle through JUCE's
VST3 host implementation:

```bat
tools\cmake\bin\cmake --build KChannel\build --config Release --target KChannelHostTest
KChannel\build\KChannelHostTest_artefacts\Release\KChannelHostTest.exe "KChannel\build\KChannel_artefacts\Release\VST3\K Channel.vst3"
```

It checks discovery, instantiation, all 37 parameters, the 1024-sample latency,
audio flow, bypass, Quell POST routing, state serialisation and editor creation.

## Licence

The plugin source links JUCE, which is dual-licensed (GPLv3 or a commercial JUCE
licence). Building and distributing it means complying with one of those. The
K Channel source itself is yours to use.
