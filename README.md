# QRTWhisper

![QRTWhisper](resources/icon.png)

Real-time subtitles for live conversations, powered by [Whisper.cpp](https://github.com/ggml-org/whisper.cpp).

QRTWhisper is a desktop application that captures the system audio output (or a microphone) and transcribes it live on the local machine, using the GPU when available. The primary use case is the real-time conversation —video call, meeting, interview— where the delay between speech and subtitle must be minimal and the resulting text accurate. It is, in essence, an accessibility tool.

The application is based on `whisper.cpp/examples/stream` and is tested on Linux + KDE.

- Repository: <https://github.com/dei-eng/qrtwhisper>
- Video demo: <https://www.youtube.com/watch?v=JTVPFc1cCBk>

## Features

- **Self-managed virtual microphone.** To transcribe the remote voice, the application creates an audio source that monitors the selected output (a `module-remap-source` over the sink's monitor), detects it among the capture devices and selects it automatically. It is destroyed on exit.
- **Interchangeable virtual-mic backends.** Two implementations live behind the `VirtualMic` interface and are selected at build time with `-DQRTWHISPER_MIC_BACKEND`:
  - `libpulse` (default): PulseAudio C API via `pa_threaded_mainloop`, no external commands.
  - `pactl`: invokes `pactl` as an external process (`QProcess`).
- **Input sources.** System output (remote voice) or a real microphone (own voice), selectable at runtime.
- **Two ways to present the transcription.** System notifications, or a semi-transparent subtitle overlay window on top of the screen.
- **Integrated model management.** A dialog lists the model catalog, shows download state, downloads with progress and cancellation, verifies the SHA-256 checksum against HuggingFace metadata, and allows deleting models.
- **Configurable models directory.** Defaults to `./models` relative to the executable (or next to the AppImage file) and is persisted; it can be changed at any time from the Model Management window.
- **Multilanguage (English / Spanish)** with runtime switching, no restart, and persistent selection.
- **System tray** with orderly shutdown: the worker is stopped and the audio module is unloaded before quitting.
- **GPU acceleration (CUDA)** with fat binaries, and a configurable architecture list.

## Architecture

The sources are organized by responsibility, mirroring the MVC/MVP design where the view does not know the model:

```
model/         model.*, Worker.*, WhisperParams.h,
               ModelManager.*, VirtualMic.h, VirtualMicFactory.*,
               PulseAudioVirtualMic.*, PactlVirtualMic.*
view/          View.*, MainWidget.*, ModelManagerDialog.*,
               TextRender.*, TransparentWindow.h, Tray.*
controller/    controller.*
resources/     resources.qrc, icon.png
translations/  translations.qrc, qrtwhisper_es.ts, qrtwhisper_es.qm
tools/         sdl_mics.cpp
main.cpp
```

- The **Model** owns the transcription lifecycle and runs the heavy inference in a `Worker` moved to a `QThread`, so the UI stays responsive.
- The **View** (widgets, subtitle overlay, tray, management dialog) only emits user intents and renders state; it does not reference the model.
- The **Controller** is the single mediator: it reads the view, calls the model and services (`VirtualMic`, `ModelManager`) and updates the view.
- Services are kept behind explicit seams: the virtual microphone behind `VirtualMic` + `VirtualMicFactory`, and model catalog/download behind `ModelManager`. This keeps them replaceable without touching the rest of the application.

## Requirements

- Linux (tested on KDE).
- Qt6 development packages (`qt6-base-dev`), including Widgets and Network.
- CMake and a C++17 compiler.
- SDL2 (`libsdl2-dev`), used for audio capture.
- `whisper.cpp` as a Git submodule (pinned to a specific commit).
- For the default `libpulse` backend: `libpulse-dev`.
- Recommended: NVIDIA driver + CUDA for real-time performance. If CUDA is not available the project still builds and runs on CPU (slower); other `whisper.cpp` GPU backends can be enabled in the submodule.

## Build

```sh
git clone --recursive https://github.com/dei-eng/qrtwhisper.git
cd qrtwhisper
cmake -S . -B build
cmake --build build -j"$(nproc)"
./build/QRTWhisper
```

If you cloned without submodules:

```sh
git submodule update --init --recursive
```

### Build options

- Virtual microphone backend:

  ```sh
  cmake -S . -B build -DQRTWHISPER_MIC_BACKEND=libpulse   # default
  cmake -S . -B build -DQRTWHISPER_MIC_BACKEND=pactl
  ```

- CUDA architectures (fat binaries; the driver picks the matching one at runtime):

  ```sh
  # Fast development build for a single GPU (e.g. GTX 16xx)
  cmake -S . -B build -DCMAKE_CUDA_ARCHITECTURES=75

  # Multi-GPU release (RTX 30xx / 40xx); add 120 with CUDA >= 12.8 (RTX 50xx)
  cmake -S . -B build -DCMAKE_CUDA_ARCHITECTURES="75;86;89"
  ```

  No runtime GPU detection is needed: a single binary built for several architectures runs on all of them. Architectures listed without the `-real`/`-virtual` suffix also embed PTX, which the driver can JIT-compile for newer GPUs.

## Models

Models are managed from the application (no terminal needed). The default directory is `./models` relative to the executable — or next to the `.AppImage` file when running as an AppImage — and is persisted in `QSettings`. If the stored directory is missing, the application falls back to the default and saves it again. It can be changed from **Manage models… → Change…**.

The catalog includes English-only (`tiny.en`, `base.en`, `small.en`, `medium.en`) and multilingual (`tiny`, `base`, `small`, `medium`, `large-v3`, `large-v3-turbo`) variants. Downloads come from HuggingFace and the SHA-256 checksum is verified against the repository metadata before the file is accepted.

## Usage

1. Run `./build/QRTWhisper`.
2. Select the **model** (download one first if the list is empty).
3. Choose the **source**: *System output (virtual mic)* to read the remote voice, or *Real microphone*.
4. Pick the **output** device to monitor (default: the active one) or the real microphone.
5. Choose the **display method**: *System notification* or *Subtitle*.
6. Select the **language** (English / Español) if needed.
7. Press **Start**. The window hides and the app stays in the tray; use **Quit** to stop the worker and unload the audio module.

To list capture devices from the terminal, the `sdl_mics` helper is also built.

### Keyboard focus note

The subtitle overlay raises and activates its window on each update, so it can take the keyboard focus. This is acceptable for a conversation where the desktop is not being used actively, but if you need to keep focus, prefer the *System notification* mode. On KDE it can help to disable *System Settings → Window Behavior → Focus → Click raises active window*.

## AppImage

A script is provided to build a self-contained AppImage using `linuxdeploy` + `linuxdeploy-plugin-qt`:

```sh
./build-appimage.sh Release
```

The models are not embedded in the image (the mount is read-only); the application stores and reads them from `./models` next to the `.AppImage`, or from the directory configured by the user. The CUDA driver is expected to be installed on the host.

## What's new in 1.1

Version 1.1 is a substantial evolution over the first version (May 2025), which was essentially a direct Qt wrapper around the `whisper.cpp` streaming example. The comparison:

| Area | 1.0 (2025, first version) | 1.1 (current) |
| --- | --- | --- |
| Code structure | Flat source tree, `stream.cpp` from the example, minimal MVC | `model/`, `view/`, `controller/` modules plus `resources/`, `translations/`, `tools/` |
| Virtual microphone | Manual: a `pactl` command documented in the README | Self-managed, auto-detected and auto-selected; two interchangeable backends (`libpulse` default, `pactl`) behind `VirtualMic` |
| Model management | Manual download via the `whisper.cpp` script; fixed path | In-app catalog, download with progress/cancel, SHA-256 verification, deletion, configurable and persisted directory |
| Display methods | On-screen text (later replaced by system notifications) | Selectable: system notifications or semi-transparent subtitle overlay |
| Language | English only, hardcoded | English + Spanish, runtime switching, persisted |
| Inference thread | Coupled to the streaming loop | `Worker` on a `QThread`, with controlled errors |
| Packaging | Manual build; hardcoded/unspecified CUDA arch | AppImage script; configurable CUDA fat binaries (`75;86;89`, optional `120`) |
| Assets | None | Application and tray icon embedded as Qt resources |
| Dependencies | `whisper.cpp` submodule, SDL2, PulseAudio | Same, plus Qt Network (models) and `libpulse` for the default backend |

In short: what was a script-driven prototype is now a self-contained application with an explicit architecture, integrated device/model management and localization.

## Roadmap

- Native PipeWire backend (`libpipewire`) behind the existing `VirtualMic` interface, without external commands.
- Improve the subtitle overlay so it does not interfere with keyboard focus.
- More languages (add a `.ts` catalog and ship its `.qm`).
- Builds for other platforms (Windows, macOS) and packaging beyond AppImage.

## License

Copyright (C) 2025 David Emmanuel Lopez.

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.

## Support

You can sponsor the project; it helps its growth.

USDT via TRC20: `TFPJT5d3aBoWcPDCGf241MZUen3E9htd23`
