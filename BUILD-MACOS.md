# Building MicSmith on macOS (10.13 High Sierra and later)

The plug-in is plain C++17 + JUCE, so the same sources build on Windows and
macOS. The only thing that differs is the toolchain — a macOS plug-in is a
Mach-O bundle and cannot be produced on Windows, so the Mac build has to be
made on a Mac.

## What you get on macOS

| Format | Who uses it |
| --- | --- |
| Audio Unit (`MicSmith.component`) | Logic Pro, GarageBand, any AU host |
| VST3 (`MicSmith.vst3`) | Live, Cubase, Studio One, Reaper, Bitwig, … |
| Standalone (`MicSmith.app`) | Auditioning without a DAW |

## Or let GitHub build it

`.github/workflows/build-macos.yml` builds the plug-in on a macOS runner and
uploads `MicSmith-VST3-macOS10.13-x86_64.zip` and
`MicSmith-AU-macOS10.13-x86_64.zip` as artifacts. Push the repository and open
the Actions tab; it also runs on pull requests and on demand
(`workflow_dispatch`). It needs the JUCE folder to be absent from the repo -
the workflow clones JUCE 8.0.6 itself.

## 1. Copy the project to the Mac

Copy the whole project folder, including `JUCE/` (about 100 MB, JUCE 8.0.6 is
vendored there). If you would rather not copy it:

```bash
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git JUCE
```

## 2. Install the build tools

```bash
xcode-select --install      # Command Line Tools
brew install cmake
```

Full Xcode is only needed if you want an Xcode project rather than Makefiles.

## 3. Build

```bash
chmod +x build_macos.sh install_macos.sh
./build_macos.sh
```

This configures with `CMAKE_OSX_DEPLOYMENT_TARGET=10.13` and
`CMAKE_OSX_ARCHITECTURES=x86_64`, then builds AU + VST3 + Standalone.

## 4. Install

```bash
./install_macos.sh
```

That copies the plug-ins into `~/Library/Audio/Plug-Ins/VST3/` and
`~/Library/Audio/Plug-Ins/Components/`, clears any quarantine flag and
ad-hoc signs them.

System-wide instead (needs `sudo`):

```
/Library/Audio/Plug-Ins/VST3/MicSmith.vst3
/Library/Audio/Plug-Ins/Components/MicSmith.component
```

Verify the Audio Unit:

```bash
auval -v aufx Msm1 Msmi
```

Then restart the DAW and rescan plug-ins.

## If the plug-in does not load on 10.13 – 10.15

This is the one real trap, and it is not specific to MicSmith.

JUCE 8 officially supports deployment targets down to macOS 10.11, but a
recent Xcode compiles against a modern SDK, and the resulting binary can call
`libc++` routines that only exist from macOS 11 onwards. The build succeeds and
`otool -l` even reports `minos 10.13`, yet the plug-in fails to load on an
actually-old Mac.

The fix is to link the C++ runtime statically:

```bash
MICSMITH_EXTRA_CMAKE_FLAGS="-DMICSMITH_LD_CLASSIC=ON" ./build_macos.sh
```

If that still does not work, build on the oldest Mac you can (10.13 or 10.14
with Xcode 13) — Apple's own guidance is that the safest way to target an old
deployment target is to build with an old SDK.

## Notes for High Sierra specifically

- **Intel only.** 10.13 runs on x86_64 Macs, so the build is x86_64. The same
  binary also runs on Apple Silicon through Rosetta, so you do not need a
  universal build for compatibility.
- **No notarisation.** Builds are unsigned/ad-hoc signed. Locally built
  plug-ins load fine; if you copy a build from another machine, strip the
  quarantine flag first (`install_macos.sh` does this).
- **Gatekeeper** may ask you to allow the standalone app the first time. Use
  right-click → Open, or System Preferences → Security & Privacy.

## Verifying the build

The same two test programs build on macOS:

```bash
./build-mac/MicSmithDSPTest            # modelling maths, no DAW needed
```

and the host-side check (excluded from the default build):

```bash
cmake --build build-mac --target MicSmithHostTest
./build-mac/MicSmithHostTest_artefacts/Release/MicSmithHostTest \
    "$HOME/Library/Audio/Plug-Ins/VST3/MicSmith.vst3"
```

The host test loads the plug-in through JUCE's VST3 host implementation and
confirms it is discovered, instantiated, exposes all 11 parameters, passes
audio, responds to model changes and serialises state.

## Using it in FL Studio 21

FL Studio on macOS loads **VST3**, so install `MicSmith.vst3` (the `.component`
is only for Logic and GarageBand).

FL Studio will not find a VST3 anywhere except the standard folder, and it does
not accept custom search paths for VST3 plug-ins:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3
cp -R build-mac/MicSmith_artefacts/Release/VST3/MicSmith.vst3 ~/Library/Audio/Plug-Ins/VST3/
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/MicSmith.vst3
```

Use `/Library/Audio/Plug-Ins/VST3/` instead to install it for every user on the
Mac (that one asks for your admin password).

Then in FL Studio: **Options > Manage plugins**, tick **Verify plugins**, click
**Find plugins**. MicSmith is an effect, so it appears in the effect list rather
than the generator list.
