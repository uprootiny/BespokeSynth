## Fork: uprootiny/BespokeSynth

[![macOS Build](https://github.com/uprootiny/BespokeSynth/actions/workflows/build-macos.yml/badge.svg)](https://github.com/uprootiny/BespokeSynth/actions/workflows/build-macos.yml)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](code_of_conduct.md)

This is a **fork** of [BespokeSynth/BespokeSynth](https://github.com/BespokeSynth/BespokeSynth), a modular software synthesizer. This fork focuses on **macOS build improvements**, Sequoia (15) compatibility, CI automation, and targeted bug fixes.

---

### Fork Additions

#### macOS Sequoia (15) JUCE Compatibility Fix

The upstream JUCE submodule uses `CGWindowListCreateImage`, which was deprecated in macOS 14 and **removed entirely** from the macOS 15 (Sequoia) SDK. This fork includes a patch (`patches/juce-macos15-cgwindowlist.patch`) that wraps the affected code in a preprocessor guard:

```c
#if defined (MAC_OS_VERSION_15_0) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_15_0
  return {};  // return empty image on macOS 15+ SDK
#else
  // original CGWindowListCreateImage code path
#endif
```

The file patched is `libs/JUCE/modules/juce_gui_basics/native/juce_Windowing_mac.mm`. The proper long-term fix is to migrate to ScreenCaptureKit (available from macOS 12.3+). In CI, the patch is applied inline via Python for the macOS 15 runner only.

#### CI Build Matrix

The GitHub Actions workflow (`.github/workflows/build-macos.yml`) builds on every push to `main`/`master` and on pull requests. The matrix includes:

| Target | Runner | Architectures | JUCE Patch Applied |
|--------|--------|---------------|--------------------|
| macOS Sonoma | `macos-14` | `arm64;x86_64` (universal) | No |
| macOS Sequoia | `macos-15` | `arm64` | Yes |

Artifacts produced per build: `.app` zip and `.dmg` (when packaging succeeds). Tagged pushes (`v*`) automatically create a GitHub Release with all artifacts attached.

#### Download Pre-built .app from GitHub Actions

1. Go to [Actions > Build macOS](https://github.com/uprootiny/BespokeSynth/actions/workflows/build-macos.yml)
2. Click the most recent successful run
3. Scroll to the **Artifacts** section at the bottom of the run page
4. Download `BespokeSynth-macos-14-app` (universal) or `BespokeSynth-macos-15-app` (arm64)

Or use the GitHub CLI:

```shell
# Download the Sonoma universal build
gh run download --repo uprootiny/BespokeSynth --name BespokeSynth-macos-14-app

# Download the Sequoia arm64 build
gh run download --repo uprootiny/BespokeSynth --name BespokeSynth-macos-15-app
```

#### Build Locally on macOS with Nix

Prerequisites: Xcode command line tools and Nix.

```shell
xcode-select --install   # required for macOS frameworks (CoreAudio, CoreMIDI, etc.)
```

Install build tools via Nix (do NOT use brew):

```shell
nix-env -iA nixpkgs.cmake nixpkgs.ninja nixpkgs.python310
```

Or use a `shell.nix`:

```nix
{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs; [ cmake ninja pkg-config python310 ];
  shellHook = ''
    export BESPOKE_PYTHON_ROOT=${pkgs.python310}
  '';
}
```

Then build:

```shell
nix-shell   # or: nix develop
git submodule update --init --recursive
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release \
  -DBESPOKE_PYTHON_ROOT="$(nix eval --raw nixpkgs#python310)/lib/python3.10"
cmake --build build --parallel 4 --config Release
# Run:
./build/Source/BespokeSynth_artefacts/Release/BespokeSynth
```

For a universal binary (arm64 + x86_64):

```shell
cmake -Bbuild -GXcode \
  -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' \
  -DCMAKE_BUILD_TYPE=Release \
  -DBESPOKE_PORTABLE=True
cmake --build build --config Release --target BespokeSynth --parallel 4
```

#### Bug Fixes Applied

- **VinylTempoControl**: fixed `mHasSignal` never being set (module was non-functional)
- **VinylTempoControl**: moved stack-allocated 16KB buffer to heap (stack overflow on Windows)
- **Canvas**: fixed memory leak in `RemoveElement`
- **Python scripting**: `me.get()`/`me.set()` now throw on invalid control paths instead of failing silently
- **Oversampling**: replaced per-sample division with multiplication (`ModularSynth.cpp:2381`)
- **EffectChain**: skip dry buffer copy when effect is 100% wet

#### Technical Documentation

- [AUDIT.md](AUDIT.md) -- code audit covering critical bugs, macOS compatibility issues, Python scripting gaps, and performance bottlenecks with a prioritized fix matrix
- [ANALYSIS.md](ANALYSIS.md) -- architecture overview, module catalog (244+ modules), Python scripting API reference, build instructions, and development roadmaps

---

*Upstream README follows below.*

---

[![Build Status](https://dev.azure.com/awwbees/BespokeSynth/_apis/build/status/BespokeSynth.BespokeSynth?branchName=main)](https://dev.azure.com/awwbees/BespokeSynth/_build/latest?definitionId=1&branchName=main)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](code_of_conduct.md)


# Bespoke Synth

A software modular synth that I've been building for myself since 2011, and now you can use it!

[Nightly Build](https://github.com/BespokeSynth/BespokeSynth/releases/tag/Nightly) (updated every commit)

You can find the most recent builds for Mac/Windows/Linux at http://bespokesynth.com, or in the [Releases](https://github.com/BespokeSynth/BespokeSynth/releases) section on GitHub.

Join the [Bespoke Discord](https://discord.gg/YdTMkvvpZZ) for support and to discuss with the community.


## Documentation

* [Official documentation](https://www.bespokesynth.com/docs/)
* [Searchable, community-written documentation](https://github.com/BespokeSynth/BespokeSynthDocs/wiki)


## Screenshot

![screenshot](screenshot-1.png)


## Basic Overview/Tutorial Video

[![Bespoke Overview](https://img.youtube.com/vi/SYBc8X2IxqM/0.jpg)](https://www.youtube.com/watch?v=SYBc8X2IxqM)
* https://youtu.be/SYBc8X2IxqM

### Quick Reference

![quick reference](bespoke_quick_reference.png)


### Features

* live-patchable environment, so you can build while the music is playing
* VST, VST3, LV2 hosting
* Python livecoding
* MIDI & OSC controller mapping
* Works on Windows, Mac, and Linux


### License

[GNU GPL v3](LICENSE)


### Releases

Sign up here to receive an email whenever I put out a new release: http://bespokesynth.substack.com/


### Contributing

[See our contributing guidelines](CONTRIBUTING.md)


### Building

Building Bespoke from source is easy and fun! The basic cmake prescription gives you a completed
executable which is ready to run on your system in many cases. If your system does not have `cmake` installed already you must do so.

```shell
git clone https://github.com/BespokeSynth/BespokeSynth   # replace this with your fork if you forked
cd BespokeSynth
git submodule update --init --recursive
cmake -Bignore/build -DCMAKE_BUILD_TYPE=Release
cmake --build ignore/build --parallel 4 --config Release
```

This will produce a release build in `ignore/build/Source/BespokeSynth_artefacts`.

There are a few useful options to the *first* cmake command which many folks choose to use.

* `-DBESPOKE_VST2_SDK_LOCATION=/path/to/sdk` will activate VST2 hosting support in your built
  copy of Bespoke if you have access to the VST SDK
* `-DBESPOKE_ASIO_SDK_LOCATION=/path/to/sdk` (windows only) will activate ASIO support on windows in your built copy of Bespoke if you have access to the ASIO SDK
* `-DBESPOKE_SPACEMOUSE_SDK_LOCATION=/path/to/sdk` (windows only) will activate SpaceMouse canvas navigation support on windows in your built copy of Bespoke if you have access to the SpaceMouse SDK
* `-DBESPOKE_PYTHON_ROOT=/...` will override the automatically detected python root. In some cases with M1 mac builds in homebrew this is useful.
* `-DCMAKE_BUILD_TYPE=Debug` will produce a build with debug information available
* `-A x64` (windows only) will force visual studio to build for 64 bit architectures, in the event this is not your default
* `-GXcode` (mac only) will eject xcode project files rather than the default make files
* `-DCMAKE_INSTALL_PREFIX=/usr` (only used on Linux) will set the `CMAKE_INSTALL_PREFIX` which guides both where your
  built bespoke looks for resources and also where it installs. After a build on Linux with this configured, you can
  do `sudo cmake --install ignore/build` and bespoke will install correctly into this directory. The cmake default is `/usr/local`.

The directory name `ignore/build` is arbitrary. Bespoke is set up to `.gitignore` everything in the `ignore` directory but you
can use any directory name you want for a build or have multiple builds also.

For building on Linux, you can also use [`just`](https://github.com/casey/just) to build by running `just build`. Use `just list` to see other options available with `just`.

To be able to build you will need a few things, depending on your OS

* All systems require an install of git
* On Windows:
    * Install Visual Studio 2019 Community Edition. When you install Visual Studio, make sure to include CLI tools and CMake, which are included in
      'Optional CLI support' and 'Toolset for Desktop' install bundles
    * Python from python.org
    * Run all commands from the visual studio command shell which will be available after you install VS.
* On MacOS: install xcode; install xcode command line tools with `xcode-select --install` and install cmake with `brew install cmake` if you use homebrew or from cmake.org if not
* On Linux you probably already have everything (gcc, git, etc...), but you will need to install required packages. The full list we
  install on a fresh ubuntu 20 box are listed in the azure-pipelines.yml
    * Some distributions may have slightly different package names like for instance Debian bookworm: You need to replace `alsa` and `alsa-tools` with `alsa-utils`
