# GoldHEN Plugins + KBM

A GoldHEN PS4 plugins repository with an added **keyboard & mouse plugin (KBM)**
that lets you control PS4 games with a USB keyboard and mouse.

> **Author of the KBM plugin and KBM Config app:**
> [**Gar113**](https://github.com/Gar113)
>
> Everything else is the original
> [GoldHEN Plugins Repository](https://github.com/GoldHEN/GoldHEN_Plugins_Repository).

---

## ⬇️ Download (ready to use)

**[`dist/kbm-bundle.zip`](dist/kbm-bundle.zip)** — plugin + config + app, already built.

Extract it and you get:

| File | Copy to | What it is |
| --- | --- | --- |
| `kbm.prx` | `/data/GOLDHEN/plugins/` | the keyboard & mouse plugin |
| `kbm.cfg` | `/data/GOLDHEN/` | sensitivity / smoothing settings |
| `kbm_keys.cfg` | `/data/GOLDHEN/` | example key mapping |
| `KBM-Config.pkg` | install on PS4 | app to rebind keys by pressing them |
| `BACA-INI.txt` | — | install instructions (Indonesian) |

---

## What the KBM plugin does

- **Keyboard + mouse work in all games.** It uses the official
  `libSceKeyboard` / `libSceMouse` APIs and hooks `scePadRead` /
  `scePadReadState`, so any game that reads the standard pad API gets the input.
- **USB hubs and hot-plug are fine.** Devices are picked up even if plugged in
  after the game started.
- **Only for the player using them.** Input is applied to player 1's pad handle
  only, so it does not leak to other players in co-op / split-screen.
- **Mouse = camera.** Mouse movement drives the right analog stick, with
  tunable sensitivity, deadzone, curve and smoothing.
- **Rebind anything.** Every controller button can be remapped, either by
  editing a config file or with the **KBM Config** app on the PS4.

### Default mapping

| Keyboard / Mouse | Controller |
| --- | --- |
| W A S D | Left stick |
| Mouse move | Right stick (camera / aim) |
| Arrow keys | D-pad |
| Space | Cross ✕ |
| E | Square ▢ |
| F | Circle ⭕ |
| Q | Triangle △ |
| R | L1 |
| T | R1 |
| Shift | L3 |
| Ctrl | R3 |
| Enter / Esc | Options |
| Tab | Touch pad button |
| Mouse left click | R2 |
| Mouse right click | L2 |
| Mouse middle click | R1 |

---

## Quick install

1. Install [GoldHEN](https://github.com/GoldHEN/GoldHEN/releases/latest) on your PS4.
2. Copy `kbm.prx` to `/data/GOLDHEN/plugins/kbm.prx`.
3. Copy `kbm.cfg` to `/data/GOLDHEN/kbm.cfg`.
4. Add `/data/GOLDHEN/plugins/kbm.prx` to `/data/GOLDHEN/plugins.ini`
   (under `[default]`).
5. Start a game — keyboard and mouse work.

To change the buttons, install `KBM-Config.pkg`
(Settings → Debug Settings → Package Installer), open **KBM Config**, press
the key you want for each button, and save.

The app has two screens, switched with **F1** / **F2**:

| Key | Screen |
| --- | --- |
| **F1** | **BUTTONS** — rebind the keyboard mapping |
| **F2** | **TEMPLATES** — save / load / delete named setting sets |

Navigation works from the left stick, the D-pad, or the arrow keys, so the
app is usable whether or not the pad is detected. `TAB`/`ENTER`/arrow keys
are enough to drive everything from the keyboard alone.

---

## Configuration

`/data/GOLDHEN/kbm.cfg`

```ini
sens=14        ; mouse / camera speed (default 14)
invert_y=0     ; 1 = inverted vertical axis
deadzone=1     ; ignore tiny movements
curve=20       ; response curve (1 = linear, 200 = very aggressive)
smooth=1       ; 0 = most responsive, 8 = smoothest
max=60         ; cap per-frame stick push (anti over-swing)
```

`/data/GOLDHEN/kbm_keys.cfg` — per-button key mapping, written by the
**KBM Config** app. Values are HID usage IDs in hex (`2C` = Space).

### Templates

Instead of re-tuning for every game, settings can be stored as named
templates and picked from the app's **TEMPLATES** screen:

    /data/GOLDHEN/templates/DEFAULT.cfg
    /data/GOLDHEN/templates/FPS.cfg
    /data/GOLDHEN/templates/Sniper.cfg
    ...

One template holds the camera settings and the key mapping together.
Selecting one writes both `kbm.cfg` and `kbm_keys.cfg`, so the plugin is
unchanged and existing config files keep working. The files are plain text
and can be copied, edited or shared. `DEFAULT` always exists and cannot be
deleted, so there is always a way back.

---

## Building from source

```sh
export OO_PS4_TOOLCHAIN=$HOME/OpenOrbis/PS4Toolchain
export GOLDHEN_SDK=$HOME/GoldHEN_Plugins_SDK

# plugin
make -C plugin_src/kbm

# homebrew app (needs OpenSSL 1.1 for PkgTool.Core, see its README)
make -C app_src/kbm_config

# rebuild the ready-to-use zip
./tools/make_kbm_bundle.sh
```

Plugin output: `bin/plugins/prx_final/kbm.prx`
App output: `app_src/kbm_config/*.pkg`

See [`.github/README.md`](.github/README.md) for the full documentation of the
original plugins.

---

## Credits

- **KBM plugin + KBM Config app:** [Gar113](https://github.com/Gar113)
- **GoldHEN Plugins, SDK and build system:**
  [bucanero](https://github.com/bucanero),
  [ctn123](https://github.com/ctn123),
  [illusion](https://github.com/illusion0001),
  [jocover](https://github.com/jocover),
  [nkrapivin](https://github.com/nkrapivin),
  [OSM](https://github.com/OSM-Made),
  [SiSTRo](https://github.com/SiSTR0)

## License

Inherits the license of the original GoldHEN Plugins repository — see
[`LICENSE`](LICENSE). The KBM plugin and KBM Config app are distributed under
the same license.

## Known limitations

- A few games (notably Capcom RE Engine titles such as RE2 / RE4 Remake) may
  not receive mouse input yet. This is being investigated.
- Key-mapping changes take effect after restarting the game.
