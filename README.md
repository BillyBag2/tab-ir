# TAB5 IR Remote Studio

ESP-IDF touchscreen application for the **M5Stack TAB5 + Unit IR (U002)** on the Grove connector. Learn a remote's message, assign it to a button type and send it later by choosing the named remote profile and button.

## Hardware

Connect the Unit IR using its straight-through Grove cable:

| Wire | TAB5 | Unit IR |
| --- | --- | --- |
| Yellow | GPIO53 | IR TX |
| White | GPIO54 | IR RX |
| Red | 5V | Supply |
| Black | GND | Ground |

The firmware enables the external 5V rail. The connector is used as two GPIOs, not I2C; do not attach an I2C hub to it while running this app. Wiring is based on the official [TAB5 pin map](https://docs.m5stack.com/en/core/Tab5) and [Unit IR pin map](https://docs.m5stack.com/en/unit/Unit_IR).

## Build and flash

Use an activated **ESP-IDF 6.1** environment with the ESP32-P4 tools. From this directory in PowerShell:

```powershell
.\tools\setup-dependencies.ps1
idf.py -B build-tab5 -D SDKCONFIG=sdkconfig.tab5 build
idf.py -B build-tab5 -D SDKCONFIG=sdkconfig.tab5 -p COM5 flash monitor
```

Replace `COM5` with the TAB5 USB port. The separate config/build directory avoids the starter project's 2 MB/no-PSRAM configuration. Defaults select the TAB5's 16 MB flash, 200 MHz PSRAM, ESP32-P4 revision 1.0, 360 MHz CPU and a 1 MB NVS partition. ESP-IDF 6.1 uses different binaries for chip revisions below 3.0 and revisions 3.x; if your hardware reports 3.x, change that revision selection with `idf.py -B build-tab5 -D SDKCONFIG=sdkconfig.tab5 menuconfig` before building.

If an existing configuration still selects a 400 MHz CPU for a pre-v3 chip, startup asserts in `esp_clk_init` even though PSRAM passes its test. Defaults do not override existing settings. Open `menuconfig` using the same build/config arguments, select **Component config > ESP System Settings > CPU frequency > 360 MHz**, save, rebuild and flash. Both `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360=y` and `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=360` should appear in the resulting configuration. The application CMake check rejects the incompatible combination before a new firmware image is built.

The dependency script fetches pinned official M5Unified, M5GFX and Espressif esp-usb revisions into `components/`. These downloaded directories are ignored by Git; the script is the reproducible dependency definition. ESP-IDF Component Manager is disabled because dependencies are supplied locally. CMake selects the USB host core and HID class components from esp-usb; their source comments and licenses remain in their checkouts.

Flashing the custom partition table changes the starter partition layout. Saved profiles are retained by subsequent firmware flashes with this same layout; an erase-flash operation removes them.

## Use

1. Tap **New**, enter a profile name (for example `RM-ED060`) on the touchscreen keyboard, then **Save name**.
2. Select a button type such as **Power** or **Volume +**, then **Learn**.
3. Aim the original remote at the receiver and briefly press its matching button within 10 seconds.
4. Tap **Test capture** to check playback, then **Save assignment**. The previous assignment stays intact until saving succeeds. **Discard** abandons the capture.
5. Later, select the profile with `<` / `>`, select the button and tap **Send**. An asterisk identifies stored messages. Selection retrieves the saved assignment; sending is explicit.

Up to **8 named profiles** each hold **24 button types** independently. Profiles and messages persist in flash across restarts. **Rename**, **Delete** and **Clear button** manage them; deletion and clearing require confirmation. Names support 32 printable ASCII characters. The touchscreen keyboard provides uppercase letters, digits, space, hyphen, underscore, period and slash; the physical keyboard also supports lowercase and shifted punctuation.

## Physical keyboards and orientation

The screen uses landscape rotation `3`, **180 degrees from the previous rotation `1`**. M5Unified transforms touch coordinates with the display rotation, so touch uses the same orientation.

Attach the **M5Stack TAB5 Keyboard (A164)** to **Ext.Port1**. The app uses its HID event mode over I2C at `0x6D`, with SDA on GPIO0 and SCL on GPIO1. This is independent of the Grove IR pins. The driver polls the keyboard with bounded work per UI update, handles a missing/disconnected keyboard without disabling touch, and probes for reconnection every two seconds. It uses the default keyboard I2C address and a US key layout. Register definitions follow M5Stack's [official keyboard driver](https://github.com/m5stack/M5Unit-KEYBOARD/blob/main/src/unit/unit_Tab5Keyboard.hpp).

Alternatively, plug a **standard USB HID boot-protocol keyboard into USB-A**. USB host mode and the port's 5V supply start automatically. The separate USB-C programming connection remains available. USB and Ext.Port1 keyboards share the controls below and may be used together; the header shows each connection separately. USB keyboards can be connected before startup or plugged in later. USB hubs are enabled, including for composite devices with a boot-keyboard interface; mouse and media-key interfaces are ignored. USB handling uses Espressif's [USB host and HID drivers](https://github.com/espressif/esp-usb).

USB input uses the US layout, supports Shift/Ctrl/Alt, Caps Lock and numeric keypad keys, and emits one action per new key press. A held key does not repeat (including Enter sends and Backspace edits). Caps Lock is tracked per keyboard in the app; keyboard LED synchronization is not implemented. Report-only/NKRO keyboards without a boot-protocol keyboard interface are unsupported; use their standard/BIOS keyboard mode if available. USB callbacks queue input for the UI and never draw on the screen. USB setup failures are shown in the header and serial log while touch and the TAB5 attachment remain usable.

| Context | Keys |
| --- | --- |
| Profile name | Type text; Backspace edits; Enter saves; Esc cancels |
| Button selection | Arrow keys move around the grid; Tab selects the next button |
| Remote selection | Ctrl+Left / Ctrl+Right, or Page Up / Page Down |
| Profile management | Ctrl+N creates; Ctrl+R renames; Ctrl+D opens delete confirmation |
| Learning | Ctrl+L starts; Esc cancels |
| Captured message | T tests; Enter or Ctrl+S saves; Esc discards |
| Saved message | Enter or Ctrl+S sends; Delete opens clear confirmation |
| Confirmation | Enter confirms; Esc cancels |

Profile/button selection is locked during learning and while a capture awaits saving, whether input comes from touch or the physical keyboard.

## Decoded IR messages

The Unit IR receiver demodulates the optical signal in hardware, and the P4's **RMT + DMA hardware** measures its mark/space timings. Software interprets that timing buffer as a protocol message, as in Espressif's [RMT NEC transceiver example](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/rmt.html). The P4 does not provide a hardware block that directly returns protocol addresses and commands.

The message line displays **protocol, bit count, address, command and raw data** for NEC, extended NEC, Samsung32 (8-bit command plus inverted command), and Sony SIRC 12/15/20-bit frames. NEC/Samsung command complements and frame timings are checked before identifying them. Example: `NEC 32-bit / Address 0x0010 / Command 0x34 / Data 0xCB34EF10`. Raw data is shown with the first received bit in bit zero. Sony20's address field includes the subdevice bits.

Unsupported or unrecognized messages are labelled **Unknown protocol / raw timings retained** and can still be tested, saved and replayed. Protocol decoding does not infer a button's meaning; assign Power/Volume/etc. yourself. Decoded fields are recomputed from the saved waveform when selected, so existing profiles work without conversion or a flash erase. Samsung and Sony timing definitions can also be inspected in the [Arduino-IRremote protocol references](https://github.com/Arduino-IRremote/Arduino-IRremote/tree/master/src).

## Message support and limits

RMT hardware captures the demodulated waveform and generates the playback carrier: 40 kHz for recognized Sony SIRC and 38 kHz otherwise. Raw replay works independently of protocol identification, but this is **single-frame learning**, not universal remote emulation. Captures allow up to 1024 alternating pulse timings; a 1 microsecond hardware glitch filter is applied, and a 12 millisecond idle ends a frame. Full/oversized captures and malformed or very short frames are rejected. The receiver cannot measure the original carrier frequency; unknown protocols use the Unit IR's nominal 38 kHz.

Long air-conditioner messages, protocols with long internal gaps, held-button repeats, and protocols with alternating toggle bits may need additional protocol-specific support. Some Sony equipment requires several repeated frames, which single-frame playback does not add; tightly spaced repeats may also be captured together and remain unrecognized. A capture's validity check confirms timing structure, not that it controls the intended appliance: use **Test capture** before saving.

## Code layout

| File | Responsibility |
| --- | --- |
| `main/main.cpp` | Board, display and main-loop setup |
| `main/remote_app.*` | Touch UI, profile selection and learning workflow |
| `main/ir_transport.*` | Grove GPIOs, RMT capture and modulated playback |
| `main/ir_decoder.*` | Timing validation, protocol identification and decoded message display |
| `main/tab5_keyboard.*` | TAB5 Ext.Port1 keyboard detection and I2C event polling |
| `main/keyboard_keys.*` | Portable HID usage/modifier to key conversion |
| `main/usb_keyboard.*` | USB-A host, HID interface lifecycle and queued keyboard input |
| `main/usb_keyboard_report.*` | Boot-report key transitions, rollover checks and Caps Lock state |
| `main/remote_model.*` | Button types, message model and validated versioned serialization |
| `main/profile_store.*` | NVS loading, committing and deleting profiles |
| `sdkconfig.defaults`, `partitions.csv` | TAB5 memory and flash layout |
| `tests/` | Portable model tests and hardware acceptance steps |

Storage errors are displayed and do not trigger an automatic flash erase. An unreadable existing profile stops startup rather than allowing its slot to be overwritten. See [tests/README.md](tests/README.md) for validation commands and the hardware checklist.
