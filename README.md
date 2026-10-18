# BMW Bluetooth Media Bridge

![Module concept](3d-design-concept.png)

Bluetooth audio for a BMW E9x (built for an E92) with control from the factory
steering-wheel buttons. A classic ESP32 is a Bluetooth A2DP receiver
that plays music through a PCM5102 DAC into the AUX input. It also listens
to the steering-wheel message on K-CAN and sends Next / Previous /
Play-Pause back to the phone over the same Bluetooth connection (AVRCP).

``` text
Phone ─A2DP─► ESP32 (WROOM-32) ─I2S─► PCM5102 ─► AUX ─► car speakers
  ▲                │ ▲
  └─────AVRCP──────┘ └─ SN65HVD230 ◄─ K-CAN (0x1D6, 100 kbit/s) ◄─ Steering-wheel buttons
```

The phone pairs with one device, "BMW E92 Audio". Volume stays with the
factory radio.

An ESP32-C3 build is also available: it sends the buttons as BLE media keys
and uses a separate Bluetooth-to-AUX receiver for audio (see
[Alternative](#alternative-if-you-already-have-a-bluetooth-to-aux-receiver)).

Current firmware: **1.6** (see [Changelog](#changelog)).

## Supported cars

| Car | Status |
|---|---|
| E9x (E90/E91/E92/E93) | Verified in an E92 |
| E8x (E81/E82/E87/E88) | Expected to work (same K-CAN and 0x1D6 buttons), not tested yet |

Needs the multifunction steering wheel. Dashboard values may differ per
model; check them with the **raw** view.

## Hardware

-   ESP32 DevKit with an ESP32-WROOM-32 (classic ESP32; the C3/S3 have no
    Bluetooth Classic, so no A2DP)
-   SN65HVD230 CAN transceiver module (3.3 V)
-   PCM5102A I2S DAC module
-   Buck converter 12 V → 5 V
-   Inline fuse 1–2 A on the 12 V input

### Wiring

| From | To | Note |
|---|---|---|
| Car +12 V | Fuse 1–2 A → Buck IN+ | Switched 12 V from the radio harness avoids battery drain |
| Car GND | Buck IN− | |
| Buck 5 V | ESP32 `VIN`/`5V`, PCM5102 `VIN` | |
| Buck GND | ESP32 GND, PCM5102 GND, SN65HVD230 GND | Common ground |
| ESP32 `3V3` | SN65HVD230 `3V3` | |
| SN65HVD230 `CTX` | ESP32 GPIO17 | CAN TX |
| SN65HVD230 `CRX` | ESP32 GPIO16 | CAN RX |
| SN65HVD230 `CANH` | K-CAN high (green) | Behind the radio |
| SN65HVD230 `CANL` | K-CAN low (orange) | Behind the radio |
| PCM5102 `BCK` | ESP32 GPIO26 | I2S bit clock |
| PCM5102 `LCK` | ESP32 GPIO25 | I2S word select (LRCK) |
| PCM5102 `DIN` | ESP32 GPIO33 | I2S data |
| PCM5102 `SCK` | GND | No master clock; the DAC's PLL makes it |
| PCM5102 `LOUT`/`ROUT`/`AGND` | AUX input (3.5 mm jack) | |

-   Remove the 120 Ω terminating resistor from the SN65HVD230 module if it
    has one; the car bus is already terminated.
-   PCM5102 module solder jumpers (back side): `FLT`, `DEMP` and `FMT` low,
    `XSMT` high (otherwise the DAC stays muted).
-   The I2S pins (BCK 26, LRCK 25, DIN 33) sit next to each other on a
    30-pin DevKit header. Change them with `I2S_*_PIN` in
    `src/app_a2dp/main.cpp`; the CAN pins are in `platformio.ini`.
-   Leave the ESP32's USB port reachable for flashing and serial logs.

K-CAN (100 kbit/s) is the green/orange twisted pair behind the radio. The
OBD port (pins 6/14) is D-CAN, 500 kbit/s, diagnostics only; the gateway
most likely does not forward 0x1D6 there. The firmware is always
**listen-only**: it never transmits or ACKs on the car bus. K-CAN is a
fault-tolerant low-speed bus; a high-speed transceiver such as the
SN65HVD230 reads it fine while both wires are intact.

## Installation

1.  Wire the ESP32, SN65HVD230 and PCM5102, create `include/secrets.h`
    (see [Wi-Fi dashboard](#wi-fi-dashboard)) and flash `app_a2dp_esp32`
    (see [Build](#build)).
2.  Connect to K-CAN behind the radio, ignition on, and open the serial
    monitor. Check for `CAN: TWAI @ 100000 bit/s listen-only`.
3.  On the phone, pair "BMW E92 Audio" in the Bluetooth settings and play
    music. Press each button: the monitor prints `MFL <button> -> AVRCP`
    (see [button mapping](#button-mapping)). The dashboard's **sniffer**
    shows all CAN IDs seen.
4.  Put it in a small enclosure (PETG/ABS, not PLA) behind the radio or in
    the armrest console, with a short AUX cable to the AUX input.
5.  Check the dashboard values with the **raw** view and correct
    `src/common/kcan_dash.cpp` if any are off.

## Button mapping

0x1D6 is 2 bytes, idle `C0 0C` (opendbc `BO_ 470 SteeringButtons`):

| Button | Byte | Mask | Frame | Action |
|---|---|---|---|---|
| Up | 0 | 0x20 | `E0 0C` | Next track |
| Down | 0 | 0x10 | `D0 0C` | Previous track |
| Vol + | 0 | 0x08 | `C8 0C` | (factory radio) |
| Vol − | 0 | 0x04 | `C4 0C` | (factory radio) |
| Telephone | 0 | 0x01 | `C1 0C` | Play/Pause |
| Voice | 1 | 0x01 | `C0 0D` | Play/Pause (on release); hold 2 s = Wi-Fi on/off |

byte0 = `0xFF` is a fault frame and is ignored. Each press sends one
command. The table lives in `src/common/mfl.cpp`.

## Wi-Fi dashboard

Both builds serve a live dashboard (speed, RPM, coolant, outside temp,
battery, fuel, range, phone connection, last button), a CAN sniffer and a
firmware update page. Code: `src/common/dashboard.*`.

**Setup**

1.  Copy `include/secrets.example.h` to `include/secrets.h` (git-ignored)
    and set the network name/password (`""` = open network) and the OTA
    password. The build stops with an error if the file is missing.
2.  Flash over USB, hold Voice 2 s and watch the serial monitor for
    `WiFi: on, network "BMW E92 Dash" at http://192.168.4.1`.
3.  On the phone join **BMW E92 Dash**, open `http://192.168.4.1` and use
    Share → Add to Home Screen.

**Usage**

-   Wi-Fi is **off** at startup. **Hold Voice 2 s** to turn it on or off
    (`w` in the serial monitor does the same). It turns off by itself
    after 10 min without use.
-   The network has no internet gateway, so the phone keeps using mobile
    data for maps. No phone hotspot needed.
-   Wi-Fi and Bluetooth share the ESP32's radio. If music stutters while
    the dashboard is open, turn Wi-Fi off again.
-   The decodings in `src/common/kcan_dash.cpp` are **not verified** yet.
    Use the **raw** button to see the frame bytes. Stale values are
    dimmed.
-   **Sniffer** (`/can`): every K-CAN ID live with changed bytes
    highlighted, rate, count, filter and pause.
-   A watchdog restarts the ESP32 if the firmware hangs for 8 s. The
    status bar shows why it last restarted (**Reset**), in red after a
    crash, watchdog or brownout.
-   For a firmware without Wi-Fi, set `-DENABLE_DASHBOARD=0` in
    `platformio.ini`. For bench tests without a car, uncomment
    `-DBENCH_FAKE_CAN=1` to feed fake frames (both builds).

**Firmware update (OTA)**

Tap **update** in the status bar (`/update`, user `admin`, password
`OTA_PASSWORD`) and upload `.pio/build/app_a2dp_esp32/firmware.bin`
(e.g. AirDrop it to the phone). Or from a computer joined to
**BMW E92 Dash**:

``` sh
pio run -e app_a2dp_esp32
curl -u admin -F "firmware=@.pio/build/app_a2dp_esp32/firmware.bin" http://192.168.4.1/update
```

`curl` asks for the password, so it stays out of the shell history. For
the C3 build, use `app_ble_esp32c3` instead.

The A2DP build uses the `min_spiffs.csv` partition table (two ~1.9 MB
app slots for OTA). Coming from an older build with `huge_app.csv`,
flash once over USB first.

## Build

PlatformIO project.

| Env | Purpose |
|---|---|
| `app_a2dp_esp32` (default) | Classic ESP32 (WROOM-32): A2DP sink → PCM5102 DAC → AUX, buttons via AVRCP, built-in CAN (TWAI) + SN65HVD230, Wi-Fi dashboard + OTA |
| `app_ble_esp32c3` | ESP32-C3 + SN65HVD230 (built-in CAN/TWAI): steering wheel → BLE media keys + Wi-Fi dashboard, audio via a separate receiver |

**macOS**

``` sh
brew install platformio
cp include/secrets.example.h include/secrets.h   # then edit it
pio run -e app_a2dp_esp32 -t upload
pio device monitor -e app_a2dp_esp32
```

The USB-serial chip (CP2102 or CH340) shows up as `/dev/cu.usbserial-*`
or `/dev/cu.wchusbserial-*`. If it doesn't, use a data cable and plug
it in directly, not via a hub's power-only port. With several serial
devices, add `--upload-port /dev/cu.usbserial-XX`.

**Windows**

``` powershell
pip install platformio
copy include\secrets.example.h include\secrets.h   # then edit it
pio run -e app_a2dp_esp32 -t upload
pio device monitor -e app_a2dp_esp32
```

The board shows up as a `COM` port (install the Silicon Labs CP210x or
WCH CH340 driver if it doesn't). With several, add `--upload-port COMx`.

## Alternative (if you already have a Bluetooth-to-AUX receiver)

`app_ble_esp32c3` reads the buttons with an ESP32-C3 (built-in CAN/TWAI)
and an SN65HVD230 and sends them to the phone as Bluetooth LE media keys.
Music plays through an existing Bluetooth-to-AUX receiver, so the phone
pairs with two devices: the audio receiver and "BMW E92 Buttons".

``` text
Steering-wheel buttons ─► K-CAN (0x1D6, 100 kbit/s) ─► SN65HVD230 ─► ESP32-C3
                                                                       │ BLE media keys
Phone ◄────────────────────────────────────────────────────────────────┘
Phone ─A2DP─► Bluetooth-to-AUX receiver ─► AUX ─► car speakers
```

**Hardware**

-   ESP32-C3-DevKitM-1 (ESP32-C3-MINI-1, micro-USB via CP2102)
-   SN65HVD230 CAN transceiver module (3.3 V)
-   Bluetooth-to-AUX receiver
-   Buck converter 12 V → 5 V
-   Inline fuse 1–2 A on the 12 V input

**Wiring**

| From | To | Note |
|---|---|---|
| Car +12 V | Fuse 1–2 A → Buck IN+ | Switched 12 V from the radio harness avoids battery drain |
| Car GND | Buck IN− | |
| Buck 5 V | C3 `5V` | |
| Buck GND | C3 GND, SN65HVD230 GND | Common ground |
| C3 `3V3` | SN65HVD230 `3V3` | |
| SN65HVD230 `CTX` | C3 GPIO5 | CAN TX |
| SN65HVD230 `CRX` | C3 GPIO4 | CAN RX |
| SN65HVD230 `CANH` | K-CAN high (green) | Behind the radio |
| SN65HVD230 `CANL` | K-CAN low (orange) | Behind the radio |

-   Remove the 120 Ω terminating resistor from the SN65HVD230 module if it
    has one; the car bus is already terminated.
-   Serial runs over the board's CP2102 bridge (UART0). For a C3 board
    with native USB only (e.g. SuperMini), set `ARDUINO_USB_MODE=1` and
    `ARDUINO_USB_CDC_ON_BOOT=1` in `[c3_twai]`.
-   An MCP2515 module (SPI) still works instead of the SN65HVD230: see
    the commented `CAN_BACKEND_MCP2515` flags in `[c3_twai]`. Its SO and
    INT outputs are 5 V and need a 2.2 kΩ / 3.3 kΩ divider to the C3, and
    its J1 terminator jumper must be removed. With INT wired, the
    sniffer's top line shows **INT OK**.

**Notes**

-   Flash `app_ble_esp32c3`, check for `CAN: TWAI @ 100000 bit/s
    listen-only`, pair "BMW E92 Buttons" and press each button: the
    monitor prints `MFL <button> -> BLE key ...`.
-   The armrest console works well: power from a dual USB adapter in the
    armrest 12 V socket (shared with the Bluetooth-AUX receiver, no buck
    converter needed), twisted K-CAN pair (~1 m) from behind the radio.
-   The C3 runs at 80 MHz with reduced Wi-Fi power to stay cool.

## Roadmap

-   Per-car profiles for the whole CAN family (E8x, E9x).
-   I-Bus family (E46, E39, E53): K-Bus transceiver and CD-changer
    emulation.
-   Kit hardware: custom PCB with protected 12 V input (fuse, reverse
    polarity, TVS) and sleep or ignition-switched power.

## Changelog

-   **1.6**: Classic ESP32 (WROOM-32) + SN65HVD230 + PCM5102 is the main
    setup: A2DP audio, buttons via AVRCP, Wi-Fi dashboard and OTA
    (`min_spiffs.csv`, flash once over USB), DAC data on GPIO33. The C3
    build uses its built-in CAN (TWAI) with an SN65HVD230 instead of the
    MCP2515 (still available as an option). Dashboard, sniffer and bench
    test shared by both builds; the status bar shows the phone connection.
-   **1.5**: Code cleanup: logger removed (the app prints button presses,
    the sniffer lists CAN IDs), 2 build environments, OTA only via
    `/update` or `curl` (ArduinoOTA removed), simpler INT check,
    `secrets.h` required for the dashboard build, Voice always acts on
    release, 8 s watchdog with the reset reason on serial and the
    dashboard.
-   **1.4**: CAN sniffer, MCP2515 INT on GPIO10, 80 MHz CPU and lower
    Wi-Fi power, macOS build setup.
-   **1.3**: Wi-Fi dashboard and OTA; hold Voice 2 s for Wi-Fi.

## References

-   <https://github.com/dzid26/opendbc-BMW-E8x-E9x>: DBC
    (`opendbc/dbc/bmw_e9x_e8x.dbc`) with the 0x1D6 definition.
-   <https://github.com/BMW-E8x-E9x/BlueGenieBMW>: E9x, MCP2515 on K-CAN
    behind the radio, Bluetooth module controlled over UART.
-   <https://github.com/meyerdominik/ESP32_bluetoothmedia>: E87, classic
    ESP32 A2DP sink + PCM5102 + K-CAN. Confirms the button codes.
-   <https://github.com/llilakoblock/bmw-e87-e90-can-bt>: ESP32-S3 + BLE
    keyboard, plus an E9x CAN ID list.
-   <https://www.loopybunny.co.uk/CarPC/can/1D6.html>: 0x1D6 details.
