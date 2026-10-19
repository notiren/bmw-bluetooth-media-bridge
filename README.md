# BMW Bluetooth Media Bridge

![Module concept](3d-design-concept.png)

Bluetooth audio for BMW E-series cars with K-CAN, controlled from the steering-wheel
buttons. Classic ESP32: A2DP → PCM5102 → AUX, buttons from K-CAN → AVRCP.

``` text
Phone ─A2DP─► ESP32 (WROOM-32) ─I2S─► PCM5102 ─► AUX ─► car speakers
  ▲                │ ▲
  └─────AVRCP──────┘ └─ SN65HVD230 ◄─ K-CAN (0x1D6, 100 kbit/s) ◄─ Steering-wheel buttons
```

Firmware **1.6**.

## Supported cars

| Car | Bus | Status |
|---|---|---|
| E9x (E90/E91/E92/E93) | K-CAN | Working |
| E8x (E81/E82/E87/E88) | K-CAN | Working |
| E6x (E60/E61/E63/E64) | K-CAN | Working |
| E7x (E70/E71/E72), E84, E89 | K-CAN | Working |
| E38, E39, E46, E53, E83, E85/E86 | I-Bus | Planned |

Needs the multifunction steering wheel.

## Hardware

-   ESP32 DevKit (WROOM-32)
-   SN65HVD230
-   PCM5102A
-   Buck 12 V → 5 V, fuse 1–2 A

| From | To | Note |
|---|---|---|
| Car +12 V | Fuse 1–2 A → Buck IN+ | Switched 12 V avoids battery drain |
| Car GND | Buck IN− | |
| Buck 5 V | ESP32 `VIN`, PCM5102 `VIN` | |
| Buck GND | ESP32 `GND`, PCM5102 `GND`, SN65HVD230 `GND` | Common ground |
| ESP32 `3V3` | SN65HVD230 `3V3` | |
| SN65HVD230 `CTX` | ESP32 GPIO17 | CAN TX |
| SN65HVD230 `CRX` | ESP32 GPIO16 | CAN RX |
| SN65HVD230 `CANH` | K-CAN high (green) | Behind the radio |
| SN65HVD230 `CANL` | K-CAN low (orange) | Behind the radio |
| PCM5102 `BCK` | ESP32 GPIO26 | I2S bit clock |
| PCM5102 `LCK` | ESP32 GPIO25 | I2S word select |
| PCM5102 `DIN` | ESP32 GPIO33 | I2S data |
| PCM5102 `SCK` | GND | DAC makes its own clock |
| PCM5102 `LOUT`/`ROUT`/`AGND` | AUX input | |

-   Remove the SN65HVD230's 120 Ω resistor.
-   PCM5102 jumpers: `FLT`/`DEMP`/`FMT` low, `XSMT` high.
-   CAN is **listen-only**; the firmware never transmits.

## Setup

1.  Copy `include/secrets.example.h` to `include/secrets.h`, set passwords.
2.  Flash `app_a2dp_esp32` ([Build](#build)), ignition on.
3.  Pair "BMW E92 Audio", press the buttons: serial prints
    `MFL <button> -> AVRCP`.

## Buttons

0x1D6, idle `C0 0C` (`src/common/mfl.cpp`):

| Button | Frame | Action |
|---|---|---|
| Up | `E0 0C` | Next |
| Down | `D0 0C` | Previous |
| Vol + / − | `C8 0C` / `C4 0C` | (factory radio) |
| Telephone | `C1 0C` | Play/Pause |
| Voice | `C0 0D` | Play/Pause; hold 2 s = Wi-Fi |

## Wi-Fi dashboard

Hold Voice 2 s, join **BMW E92 Dash**, open `http://192.168.4.1`.  
Car values, CAN sniffer (`/can`) and OTA (`/update`, user `admin`).  
Turns off after 10 min. Turn Wi-Fi off if music stutters.

``` sh
curl -u admin -F "firmware=@.pio/build/app_a2dp_esp32/firmware.bin" http://192.168.4.1/update
```

Flags: `-DENABLE_DASHBOARD=0` (no Wi-Fi), `-DBENCH_FAKE_CAN=1` (fake frames).

## Build

``` powershell
pip install platformio
copy include\secrets.example.h include\secrets.h
# macOS: brew install platformio
# macOS: cp include/secrets.example.h include/secrets.h
pio run -e app_a2dp_esp32 -t upload
pio device monitor -e app_a2dp_esp32
```

| Env | |
|---|---|
| `app_a2dp_esp32` (default) | ESP32: A2DP audio + AVRCP buttons |
| `app_ble_esp32c3` | ESP32-C3: BLE media keys, separate BT-AUX receiver |

Upgrading from before 1.6: flash once over USB (new partition table).

## Alternative: ESP32-C3

Buttons as BLE media keys ("BMW E92 Buttons"); an existing Bluetooth-AUX
receiver plays the music.

| From | To | Note |
|---|---|---|
| Buck 5 V | C3 `5V` | |
| Buck GND | C3 `GND`, SN65HVD230 `GND` | Common ground |
| C3 `3V3` | SN65HVD230 `3V3` | |
| SN65HVD230 `CTX` | C3 GPIO5 | CAN TX |
| SN65HVD230 `CRX` | C3 GPIO4 | CAN RX |
| SN65HVD230 `CANH` | K-CAN high (green) | Behind the radio |
| SN65HVD230 `CANL` | K-CAN low (orange) | Behind the radio |

Native-USB C3 boards (e.g. SuperMini): set `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1`.

## Roadmap

-   I-Bus cars with CD-changer emulation
-   Custom PCB with protected 12 V input

## Changelog

-   **1.6**: ESP32 + PCM5102 is the main build; dashboard/OTA on both; C3 on TWAI.
-   **1.5**: Cleanup, watchdog, OTA via `/update` only.
-   **1.4**: CAN sniffer, cooler C3.
-   **1.3**: Wi-Fi dashboard and OTA.

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
