# STM32H7 Codex Pet

A small desktop AI-status pet built with an STM32H743 board, a 240×240 SPI IPS screen, an SD card, and an ESP8266/ESP-12F WiFi module.

The STM32 displays an animated pet from RGB565 frame files stored on the SD card. The ESP8266 runs a tiny HTTP server and forwards state changes to the STM32 over UART. Claude Code hooks can call the ESP8266 HTTP endpoints so the pet reacts to the current Claude workflow state.

## Features

- 240×240 IPS LCD pet display
- SD-card based RGB565 animation frames
- Multiple pet actions selected by state
- ESP8266 HTTP control endpoint
- UART protocol between ESP8266 and STM32
- Claude Code hook integration for automatic state sync
- Keil MDK project for STM32H743

Current state/action mapping:

| State | UART message | Pet action folder | Meaning |
|---|---|---|---|
| Idle | `STATE:IDLE` | `INNI050/I` | normal idle animation |
| Thinking | `STATE:THINKING` | `INNI050/WT` | waiting/thinking animation, skipping `WT04` |
| Answering | `STATE:ANSWERING` | `INNI050/WV` | waving animation |
| Busy | `STATE:BUSY` | `INNI050/RN` | running/busy animation |

## System Architecture

```text
Claude Code hooks / browser
        |
        | HTTP GET
        v
ESP8266 / ESP-12F
        |
        | UART, 9600 baud
        v
STM32H743
        |
        | SPI LCD + SD card frames
        v
240×240 IPS screen
```

The STM32 does not connect to WiFi directly. The ESP8266 handles WiFi and HTTP, then sends simple text commands to the STM32. This keeps the STM32 firmware simple and makes debugging easier.

## Hardware

Main hardware used:

- STM32H743VGT6 development board
- 240×240 SPI IPS LCD
- SD card / SDMMC wiring
- ESP8266MOD / ESP-12F running NodeMCU Lua firmware
- ST-Link V2 for programming the STM32
- USB-TTL adapter for configuring ESP8266

## Wiring

### LCD wiring

```text
LCD VCC -> STM32 3V3
LCD GND -> STM32 GND
LCD SDA -> STM32 SDI / PB15
LCD SCL -> STM32 SCL / PB13
LCD DC  -> STM32 D/C / PB1
LCD BLK -> STM32 BLK / PB0
LCD RES -> STM32 SDO / PB14
LCD CS  -> not connected
```

### LCD buttons

```text
K1 -> PB3
K2 -> PB5
K3 -> PB7
K4 -> PB9
```

### SDMMC wiring

```text
D0      -> PC8
D1      -> PC9
D2      -> PC10
D3      -> PC11
CLK/SCK -> PC12
CMD     -> PD2
```

### ESP8266 to STM32 UART wiring

Final UART wiring:

```text
ESP8266 / ESP-12F          STM32H743
-----------------          ---------
TX / U0TXD        ------->  PA10 / USART1_RX
RX / U0RXD        <-------  PA9  / USART1_TX
GND               -------   GND
3V3 / VCC         -------   3.3V stable power
EN / CH_PD        -------   3.3V pull-up
RST               -------   3.3V pull-up, pull to GND to reset
GPIO0             -------   3.3V for normal boot, GND for flashing firmware
```

If using a NodeMCU-style ESP8266 development board, power, EN, RST, and GPIO0 are usually handled by the board. In that case, only `TX`, `RX`, and `GND` are needed for STM32 communication.

When debugging with a USB-TTL adapter, avoid driving ESP RX from two TX pins at once. A safe initial debug wiring is:

```text
USB-TTL TX -> ESP RX
USB-TTL RX <- ESP TX
ESP TX     -> STM32 PA10 / USART1_RX
GND        -> common GND
```

After the one-way link works, connect STM32 `PA9 / USART1_TX` to ESP `RX` if needed.

## UART Protocol

The ESP8266 sends simple line-based state messages to the STM32:

```text
STATE:IDLE\r\n
STATE:THINKING\r\n
STATE:ANSWERING\r\n
STATE:BUSY\r\n
```

The STM32 firmware searches for keywords such as `IDLE`, `THINK`, `ANSWER`, and `BUSY`, then updates the displayed text and pet action.

UART settings:

```text
Baud: 9600
Data bits: 8
Parity: none
Stop bits: 1
```

## SD Card Asset Layout

Pet frames are stored as RGB565 big-endian raw `.BIN` files on the SD card.

Expected layout:

```text
/INNI050/
├─ I/
│  ├─ I00.BIN
│  ├─ I01.BIN
│  └─ ...
├─ WT/
│  ├─ WT00.BIN
│  └─ ...
├─ WV/
│  ├─ WV00.BIN
│  └─ ...
├─ RN/
│  ├─ RN00.BIN
│  └─ ...
├─ RR/
├─ RL/
├─ JP/
├─ FL/
├─ RV/
├─ PETMANI.JSON
└─ INNI_FRAMES.H
```

For the current 50% pet variant:

```text
Frame size: 96 × 104
Format: RGB565_BE_RAW
Background: transparent composited to black
```

## STM32 Firmware

Open the Keil project:

```text
USER/TIMER.uvprojx
```

Build target:

```text
TIMER
```

Important firmware behavior:

- Initializes LCD, keys, SD card, and USART1.
- Mounts the FAT32 SD card.
- Displays pet frames from `/INNI050/...`.
- Receives ESP8266 state messages via USART1.
- Updates status text and animation folder according to state.
- Refreshes only small screen regions to reduce flicker.

## ESP8266 Setup

The ESP8266 module used here runs NodeMCU Lua firmware. On boot it may print:

```text
NodeMCU 0.9.6 build 20150704 powered by Lua 5.1.4
lua: cannot open init.lua
>
```

That is normal before uploading `init.lua`.

### Basic serial test

Connect USB-TTL to ESP8266 and open a serial terminal at 9600 baud.

Test Lua REPL:

```lua
print("hello")
```

Expected output:

```text
hello
>
```

### Configure WiFi manually

Send one line at a time:

```lua
wifi.setmode(wifi.STATION)
wifi.sta.config("your-ssid", "your-password")
wifi.sta.connect()
```

Check IP:

```lua
print(wifi.sta.getip())
```

A successful result looks like:

```text
10.254.201.130    255.255.255.0    10.254.201.73
```

Status code `5` means connected:

```lua
print(wifi.sta.status())
```

### Upload HTTP bridge script

The ESP8266 bridge script is:

```text
esp8266_http_state_init.lua
```

It should be uploaded to the ESP8266 filesystem as:

```text
init.lua
```

This repo includes helper upload scripts:

```text
upload_nodemcu_lua.py
upload_nodemcu_lua_safe.py
```

Example upload command:

```powershell
python upload_nodemcu_lua_safe.py --port COM12 --baud 9600 --src esp8266_http_state_init.lua --dest init.lua
```

After restart, the ESP8266 should print:

```text
Connecting WiFi...
WiFi waiting...
WiFi connected: 10.254.201.130
HTTP server started
Try: http://10.254.201.130/state?value=thinking
```

## HTTP API

Once the ESP8266 is online, control the STM32 pet through HTTP:

```text
http://<esp-ip>/state?value=thinking
http://<esp-ip>/state?value=answering
http://<esp-ip>/state?value=idle
http://<esp-ip>/state?value=busy
```

Short paths are also supported:

```text
http://<esp-ip>/thinking
http://<esp-ip>/answering
http://<esp-ip>/idle
http://<esp-ip>/busy
```

Example response:

```text
OK STATE:THINKING
```

## Claude Code Hook Integration

A local PowerShell helper can send state changes to the ESP8266:

```powershell
$ErrorActionPreference = 'SilentlyContinue'
$Base = 'http://10.254.201.130/state?value='
$State = if ($args.Count -gt 0) { $args[0] } else { 'idle' }
try {
    Invoke-WebRequest -UseBasicParsing -TimeoutSec 1 -Uri ($Base + $State) | Out-Null
} catch {
}
```

Example Claude Code hook mapping:

- `UserPromptSubmit` -> `thinking`
- `Stop` -> `idle`
- `Notification` -> `busy`

This makes the physical pet react automatically when a Claude Code session starts processing or returns to idle.

## Notes

- ESP8266 only supports 2.4 GHz WiFi.
- If the ESP Lua REPL stops responding while connected to STM32, check whether two TX pins are both connected to ESP RX.
- If STM32 does not react to UART data, first verify the physical PA10 pin and common ground.
- `Thinking` currently skips `WT04`; playback goes `WT00 -> WT01 -> WT02 -> WT03 -> WT05`.
