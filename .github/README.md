# GoldHEN Plugins
Plugins allows you to customize your games like never before!

[![Build modules](https://github.com/GoldHEN/GoldHEN_Plugins_Repository/actions/workflows/CI.yml/badge.svg?branch=main&event=push)](https://github.com/GoldHEN/GoldHEN_Plugins_Repository/actions/workflows/CI.yml)

## Features:

- Hook into Games before boot.
- Write your own code into games in C/C++.
- Hook and modify system functions.

## :warning: Disclaimer

While we make every effort to deliver high quality products, we do not guarantee that our products are free from defects. Our software is provided **as is** and you use the software at your own risk.

# Quick Start

- Load [GoldHEN 2.3](https://github.com/GoldHEN/GoldHEN/releases/latest) or newer on your PS4.
- Enable option to load plugins in Plugins Menu.
- Download plugins from [Release Page](https://github.com/GoldHEN/GoldHEN_Plugins_Repository/releases/latest).
- Extract downloaded plugins into `/data/GoldHEN/plugins/`.
- Add plugins you want to load into `/data/GoldHEN/plugins.ini`.
  - Per game plugins sections are recommended over putting everything in `default`.

```ini
; Load plugins for any title

[default]
/data/GoldHEN/plugins/game_patch.prx

; Load plugins only for Playroom

[CUSA00001]
/data/GoldHEN/plugins/afr.prx
/data/GoldHEN/plugins/no_share_watermark.prx
```

## Plugins

### AFR (Application File Redirector)

Plugin filename: `afr.prx`

Author(s):
- [SiSTR0](https://github.com/SiSTR0)
- [jocover](https://github.com/jocover)

Redirect file calls to `/data/`

<details>
<summary>How To Use (Click to Expand)</summary>

- Create a folder in `/data`
  - `/data/GoldHEN/AFR/(title id)/`
- Place files into newly created `(title id)` folder
  - Example for `CUSA00001` `/app0/hello_afr.txt` -> `/data/GoldHEN/AFR/CUSA00001/hello_afr.txt`
- Run your game.

</details>

### Button Swap

Plugin filename: `button_swap.prx`

Author(s):
- [jocover](https://github.com/jocover)

Swap X and O around on Asia Region console.

### Force 1080p Display

Plugin filename: `force_1080p_display.prx`

Author(s):
- [illusion](https://github.com/illusion0001)

Reports to the app that it is connected to a 1080p display.

### Force 30 FPS

Plugin filename: `force_30_fps.prx`

Author(s):
- [illusion](https://github.com/illusion0001)

Force any game to run at 30 FPS.

### Fliprate Remover

Plugin filename: `fliprate_remover.prx`

Author(s):
- [illusion](https://github.com/illusion0001)

Removes framerate limit for games using system function `sceVideoOutSetFlipRate`.

### FrameTime Logger

Plugin filename: `frame_logger.prx`

Author(s):
- [illusion](https://github.com/illusion0001)

  - Log frametime statistics.
  - Press `L3 + L1 + Triangle` to start capturing data.
  - Press `L3 + R3 + L1 + R1 + Square` to stop plugin.
  - Logs data to `/data/frame_logger/`.

### GamePad helper Plugin

Plugin filename: `gamepad_helper.prx`

Author(s):
- [jocover](https://github.com/jocover)

#### Features:
- Custom DeadZone settings
- Custom TouchPad Button
- Custom Button Mapping
- Custom Viration Intensity settings

#### Getting Started
Config File in `/data/GoldHEN/gamepad.ini`.

```ini
; Load gamepad config for any title

[default]
enableDeadZone=1
DeadZoneLeft=13
DeadZoneRight=13

enableCustomTouchPad=1
TOUCH_L2=BUTTON_L3
TOUCH_R2=BUTTON_TOUCH_PAD

; Load gamepad config for Playroom

[CUSA00001]
enableCustomButton=1
BUTTON_L3=BUTTON_CROSS

VirationIntensity=medium
```

### Configuration Options
#### DeadZone setting:
- enableDeadZone:`value: 0,1`
- DeadZoneLeft:` value: 0~128`
- DeadZoneRight:`value: 0~128`

#### Custom TouchPad setting:
- enableCustomTouchPad:  `value: 0,1`
#### Support TouchPad Button Mapping list

```
TOUCH_L1
TOUCH_R1
TOUCH_L2
TOUCH_R2
```

#### TouchPad Button Map

| **L1** | &emsp;&emsp;&emsp; | **R1** |
|--------|--------------------|--------|
| &emsp; | &emsp;             | &emsp; |
| **L2** | &emsp;&emsp;&emsp; | **R2** |

#### Custom Button setting:
- enableCustomButton: `value: 0,1`
##### Support Button Mapping list

```
BUTTON_OPTIONS
BUTTON_UP
BUTTON_RIGHT
BUTTON_DOWN
BUTTON_LEFT
BUTTON_L3
BUTTON_R3
BUTTON_L2
BUTTON_R2
BUTTON_L1
BUTTON_R1
BUTTON_TRIANGLE
BUTTON_CIRCLE
BUTTON_CROSS
BUTTON_SQUARE
BUTTON_TOUCH_PAD
```

#### Viration Intensity setting:
- VirationIntensity: `options: off,weak,medium,strong`

### Game Patch

Plugin filename: `game_patch.prx`

Author(s):
- [illusion](https://github.com/illusion0001)

Patches game before boot. Using patches from the [Patch Repository](https://github.com/GoldHEN/GoldHEN_Patch_Repository)

<details>
<summary>How To Use (Click to Expand)</summary>

#### Manual Installation
- Download [patch zip](https://github.com/GoldHEN/GoldHEN_Patch_Repository/raw/gh-pages/patch1.zip)
- Extract it to `/data/GoldHEN/`

#### Easy Installation
- Patches can be configured, install/update via:
  - [GoldHEN Cheat Manager](https://github.com/GoldHEN/GoldHEN_Cheat_Manager/releases/latest)
  - [Itemzflow Game Manager](https://github.com/LightningMods/Itemzflow)
- Run your game.

</details>

##### Libraries used

- [mxml](https://github.com/bucanero/mxml)

### Keyboard & Mouse (KBM)

Plugin filename: `kbm.prx`

Author(s):
- [Gar113](https://github.com/Gar113)

Emulates a PS4 controller from a USB keyboard and mouse. Works with all games — the plugin hooks `scePadRead`/`scePadReadState`, so any game that reads the standard pad API receives the input. USB hubs are fine; the OS enumerates keyboards/mice through them just like direct connections. Devices plugged in after game start are picked up automatically (retry every ~120 polls).

Keyboard input is applied **only to player 1** (the pad handle of the user who owns the keyboard/mouse), so it does not leak into other players in co-op or split-screen games.

<details>
<summary>Default Key Mapping (Click to Expand)</summary>

| Keyboard / Mouse | Controller |
| --- | --- |
| W A S D | Left stick |
| Mouse move | Right stick (aim) |
| Arrow keys | D-pad |
| Space | Cross ✕ |
| E | Square ▢ |
| F | Circle ⭕ |
| Q | Triangle △ |
| R | L1 |
| T | R1 |
| Left/Right Shift | L3 |
| Left/Right Ctrl | R3 |
| Enter / Esc | Options |
| Tab | Touch pad button |
| Mouse left button | R2 (analog 255) |
| Mouse right button | L2 (analog 255) |
| Mouse middle button | R1 |

</details>

<details>
<summary>Configuration File (Click to Expand)</summary>

Optional file: `/data/GoldHEN/kbm.cfg`

```ini
sens=14        ; 1-200, sensitivitas mouse (default 14)
invert_y=0     ; 1 = balik sumbu vertikal
deadzone=1     ; 0-20, gerakan di bawah ini diabaikan
curve=20       ; 1-200, kelengkungan respons (kecil = linear)
smooth=1       ; 0-8, perataan gerakan (0 = mentah, responsif)
max=60         ; 10-127, batas dorongan stik per frame (anti over-swing)
```

| Opsi | Default | Fungsi |
| --- | --- | --- |
| `sens` | 14 | Kecepatan kamera. Kecil = presisi/sniper, besar = cepat |
| `invert_y` | 0 | `1` untuk kamera terbalik (inverted) |
| `deadzone` | 1 | Abaikan gerakan kecil supaya kamera tidak bergetar |
| `curve` | 20 | `1` = hampir linear (paling mirip mouse PC), `200` = sangat akseleratif |
| `smooth` | 1 | Perataan gerakan; `0` = paling responsif tapi kasar |
| `max` | 60 | Batas dorongan stik per frame; `127` = tanpa batas |

Setelan siap pakai (lihat komentar di file `kbm.cfg`):
- **FPS kompetitif**: `sens=20 curve=10 smooth=0 deadzone=1 max=80`
- **Sniper/presisi**: `sens=8 curve=60 smooth=2 deadzone=2 max=40`
- **Paling mirip mouse PC**: `sens=28 curve=1 smooth=0 deadzone=0 max=100`

Ubah nilai lalu restart game (config dibaca saat plugin dimuat).

</details>

<details>
<summary>Custom Key Mapping (Click to Expand)</summary>

Optional file: `/data/GOLDHEN/kbm_keys.cfg`

Every controller button can be remapped. Values are **HID usage IDs in hex** (e.g. `2C` = Space, `04` = A, `52` = Up arrow).

```ini
cross=2C       ; Space
square=08      ; E
circle=09      ; F
triangle=14    ; Q
l1=15          ; R
r1=1C          ; T
l3=E1          ; Left Shift
r3=E0          ; Left Ctrl
options=28     ; Enter
touchpad=2B    ; Tab
up=52
down=51
left=50
right=4F
; stick_l=     (leave empty to use WASD)
; stick_r=     (leave empty to use mouse movement)
; l2=          (leave empty to use right mouse button)
; r2=          (leave empty to use left mouse button)
```

The easiest way to edit this file is with the **KBM Config** homebrew app (see below), which writes it for you. If the file is missing or a line is absent, the built-in default is used.

Changes take effect after restarting the game.

</details>

### KBM Config (homebrew app)

App file: `KBM-Config.pkg` (inside the release zip / `dist/` folder)

Author(s):
- [Gar113](https://github.com/Gar113)

A lightweight homebrew application that lets you rebind every controller button by simply pressing the keyboard key you want — no text editor needed. It writes `/data/GOLDHEN/kbm_keys.cfg`, which `kbm.prx` reads on load.

Install with **Settings → Debug Settings → Package Installer** (requires GoldHEN), then open **KBM Config** from the PS4 menu.

| Pad button | Action |
| --- | --- |
| D-pad Up/Down | Select button |
| Cross ✕ | Rebind (then press the keyboard key) |
| Circle ⭕ | Cancel while waiting for input |
| Options | Save to `kbm_keys.cfg` |

### No Share Blocks

Plugin filename: `no_share_watermark.prx`

Author(s):
- [illusion](https://github.com/illusion0001)
- [nik](https://github.com/nkrapivin)

Removes image watermark, video and screenshot blocks from games.

# Building

## Requirements

- [LLVM 10+](https://llvm.org/)
- [OpenOrbis SDK](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)
- [GoldHEN Plugin SDK](https://github.com/GoldHEN/GoldHEN_Plugins_SDK)
- Submodules checkout with `git submodule update --init`

## Build

### Linux

1. Export path to OpenOrbis SDK.

```
export OO_PS4_TOOLCHAIN=[directory of installation]
```
2. Export path to GoldHEN SDK.

```
export GOLDHEN_SDK=[directory of installation]
```
3. Run `make` in the root of the repository or `make` individually; built binaries can be found in `bin/plugins`.

<!-- Win32 build script not up to date-->
### Windows

1. Set environment variable to OpenOrbis and GoldHEN SDK
2. Run `build_static.bat` in GoldHEN SDK repository to build the library statically.
3. Run `build.bat` to build all the modules at once.

# Coded by:

- [bucanero](https://github.com/bucanero)
- [ctn123](https://github.com/ctn123)
- [illusion](https://github.com/illusion0001)
- [jocover](https://github.com/jocover)
- [nkrapivin](https://github.com/nkrapivin)
- [OSM](https://github.com/OSM-Made)
- [SiSTRo](https://github.com/SiSTR0)

# KBM Plugin Credits

The `kbm.prx` plugin and the **KBM Config** application are not part of the
original GoldHEN Plugins collection — they are an independent addition:

- **Author:** [Gar113](https://github.com/Gar113)
  - `kbm.prx` — keyboard & mouse to controller plugin
  - `KBM-Config.pkg` — homebrew app to rebind the keys
  - `/data/GOLDHEN/kbm.cfg` and `/data/GOLDHEN/kbm_keys.cfg` config format

Built on top of the [GoldHEN Plugins SDK](https://github.com/GoldHEN/GoldHEN_Plugins_SDK)
and the [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain).
Thanks to the GoldHEN Plugins authors listed above for the SDK, hook
utilities and the build system that this plugin relies on.

# License

This repository inherits the license of the original
[GoldHEN_Plugins_Repository](https://github.com/GoldHEN/GoldHEN_Plugins_Repository).
See the [`LICENSE`](LICENSE) file for details. The `kbm` plugin and
**KBM Config** app are distributed under the same license.
