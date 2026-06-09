# Video Playback Integration (raylib-media)

## Branch Strategy (CONDITIONS)

| Role | Branch | Remote |
|------|--------|--------|
| **Work branch** (development) | `nisko/feat/video-playback` | Local only -- NEVER push to remote |
| **PR source branch** (merge target) | `nisko/feat/video-playback-PR` | Push to remote when user approves |
| **Merge flow** | Work branch -> PR branch (after approval) -> PR to `main` | |

**Rules**:
1. All development on `nisko/feat/video-playback` (local only).
2. Merge work branch into PR branch ONLY after user approval.
3. Only `nisko/feat/video-playback-PR` is pushed to remote origin.
4. Work branch stays local at all times.

---

## TL;DR

> **Quick Summary**: Integrate video playback into the raylib RPG game using [raylib-media](https://github.com/cloudofoz/raylib-media) (v0.3beta, Zlib license), an FFmpeg-backed raylib extension. Download 3 source files + FFmpeg dev DLLs via setup scripts, add a `VideoPlayer` wrapper class with play/pause/stop/seek/volume API, wire into a new `VideoScreen` game state for cutscenes, and add a "Video Volume" slider to the existing audio settings tab.

> **Deliverables**:
> - `setup.ps1` + `setup.sh` updated with `Install-RaylibMedia` + `Install-FFmpeg` functions
> - `lib/raylib-media/` (raymedia.h, rmedia.c, FindFFMPEG.cmake) -- downloaded, not tracked
> - `lib/ffmpeg/` (FFmpeg dev DLLs + headers) -- downloaded, not tracked
> - `CMakeLists.txt` updated to compile rmedia.c and link FFmpeg libs
> - `include/media/videoPlayer.h` + `src/media/videoPlayer.cpp` -- C++ wrapper class
> - `include/ui/audioTab.h` updated -- add videoVolume to SliderState
> - `src/ui/audioTab.cpp` updated -- add Video Volume slider (4th row)
> - Optional: `VideoScreen` integration for cutscene playback
> - `.gitignore` -- add lib/raylib-media/, lib/ffmpeg/

> **Estimated Effort**: Medium
> **Parallel Execution**: YES -- setup scripts (ps1 + sh) can run concurrently, CMake changes independent of audio tab changes
> **Critical Path**: Setup scripts -> CMake config -> VideoPlayer wrapper -> UI integration

---

## Analysis Summary

### Why raylib-media over libvlcpp

| Criterion | raylib-media | libvlcpp |
|-----------|-------------|----------|
| Build integration | CMake native, single .c file | Meson, custom frame extraction pipeline |
| Dependency size | ~20-40MB (FFmpeg DLLs) | ~200MB+ (VLC runtime) |
| raylib API fit | Native Texture/AudioStream output | Raw AV frames |
| License | Zlib (matches project) | LGPL-2.1+ |
| Effort to integrate | Low | High |
| FFmpeg usage | Direct (libavcodec/libavformat/etc.) | Indirect (via VLC) |

**raylib-media uses FFmpeg internally** (libavcodec, libavformat, libavutil, libswresample, libswscale -- confirmed in `rmedia.c` includes). Your concern about codec coverage is addressed.

### Capabilities

**Supported formats**: Whatever the linked FFmpeg build supports. Standard LGPL BtbN build covers MP4/H.264, WebM/VP8/VP9, MKV, MOV, AVI, OGV -- all common game cutscene formats.

**API surface** (from `raymedia.h`):
| Function | Purpose |
|----------|---------|
| `LoadMedia(fileName)` | Load video file |
| `LoadMediaEx(fileName, flags)` | Load with flags (no audio, no video, loop, no autoplay) |
| `LoadMediaFromStream(reader, flags)` | Load from custom stream (archive, network) |
| `IsMediaValid(media)` | Check if loaded successfully |
| `GetMediaProperties(media)` | Get duration, FPS, hasVideo, hasAudio |
| `UpdateMedia(&media)` | Advance playback (call each frame) |
| `UpdateMediaEx(&media, deltaTime)` | Advance with custom delta (speed control) |
| `GetMediaState(media)` | PLAYING, PAUSED, STOPPED, INVALID |
| `SetMediaState(media, newState)` | Play, pause, stop |
| `GetMediaPosition(media)` | Current playback position (seconds) |
| `SetMediaPosition(media, timeSec)` | Seek to position |
| `SetMediaLooping(media, bool)` | Enable/disable loop |
| `UnloadMedia(&media)` | Cleanup |

**Looping**:
- ON: On EOF, library seeks to position 0, flushes codec buffers, resumes playback automatically
- OFF: On EOF, state becomes `MEDIA_STATE_STOPPED`, last frame frozen on texture
- Detect end: `GetMediaState(media) == MEDIA_STATE_STOPPED`

**Volume control**: `MediaStream.audioStream` is a raylib `AudioStream`. Control via native `SetAudioStreamVolume(media.audioStream, vol)`.

---

## Dependency Files Needed

### raylib-media source (3 files)
Pinned to commit `f4bd988` on main:
- `https://raw.githubusercontent.com/cloudofoz/raylib-media/f4bd988/src/raymedia.h` -> `lib/raylib-media/raymedia.h`
- `https://raw.githubusercontent.com/cloudofoz/raylib-media/f4bd988/src/rmedia.c` -> `lib/raylib-media/rmedia.c`
- `https://raw.githubusercontent.com/cloudofoz/raylib-media/f4bd988/CMakeModules/FindFFMPEG.cmake` -> `lib/raylib-media/FindFFMPEG.cmake`

### FFmpeg dev package (Windows)
BtbN FFmpeg LGPL shared build (as recommended by raylib-media README):
- `https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n7.1-latest-win64-lgpl-shared-7.1.zip`
- Extract to `lib/ffmpeg/` with subdirs: `include/`, `lib/`, `bin/`

### FFmpeg dev package (Linux)
System packages via apt:
- `libavcodec-dev`, `libavformat-dev`, `libavutil-dev`, `libswresample-dev`, `libswscale-dev`

---

## Work Breakdown

### Wave 1 -- Setup Scripts
- [ ] Add `Install-RaylibMedia()` to `setup.ps1` (download 3 raw files from GitHub)
- [ ] Add `Install-FFmpeg()` to `setup.ps1` (download BtbN zip, extract to lib/ffmpeg/)
- [ ] Wire both into main execution flow
- [ ] Same for `setup.sh` (Linux: curl for raylib-media, apt for FFmpeg deps)
- [ ] Add `lib/raylib-media/` and `lib/ffmpeg/` to `.gitignore`

### Wave 2 -- CMake Integration
- [ ] Add `lib/raylib-media/` to include directories
- [ ] Add `rmedia.c` to source file list (or compile as separate target)
- [ ] Add FindFFMPEG.cmake and locate FFmpeg libraries
- [ ] Add FFmpeg include/lib linking (Windows: explicit paths; Linux: pkg-config)
- [ ] Copy FFmpeg DLLs to output dir post-build (same pattern as raylib.dll)
- [ ] Platform-conditional: WIN32 vs UNIX

### Wave 3 -- VideoPlayer Wrapper Class
- [ ] `include/media/videoPlayer.h` -- class wrapping MediaStream
- [ ] `src/media/videoPlayer.cpp` -- implementation
- [ ] API: Load/Unload, Play/Pause/Stop, Seek, SetLooping, SetVolume, GetPosition, GetDuration, IsFinished, Update/Draw
- [ ] Volume integration with AudioManager (respect master volume)

### Wave 4 -- Video Volume in Audio Tab
- [ ] Update `SliderState` in `audioTab.h` -- add `videoVolume` field
- [ ] Update `SLIDER_LABELS` and `ROW_OFFSETS` in `audioTab.cpp` -- add "Video Volume"
- [ ] Update `LoadAudioSettings` / `SaveAudioSettings` -- include video volume
- [ ] Wire `SetAudioStreamVolume()` to video volume when video is active

### Wave 5 -- VideoScreen (Cutscene System)
- [ ] New ScreenState: `VIDEO` in `screen.h`
- [ ] `src/ui/videoScreen.cpp` + header -- fullscreen video playback with skip prompt
- [ ] `screen_handler.cpp` -- handle VIDEO state transition
- [ ] On video end: transition to next screen (MAIN_MENU, PLAY, etc.)

---

## Verification

- [ ] Build passes on Windows (CMake + Ninja)
- [ ] Build passes on Linux (WSL, CMake + Ninja)
- [ ] FFmpeg DLLs copied to output dir post-build
- [ ] `LoadMedia()` succeeds with test .mp4 file
- [ ] `UpdateMedia()` + `DrawTexture()` renders video frames
- [ ] Looping: video restarts automatically when `SetMediaLooping(true)`
- [ ] Non-looping: `GetMediaState()` returns STOPPED at end
- [ ] Volume slider controls video audio stream
- [ ] No regressions in existing audio/music/sfx playback
