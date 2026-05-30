# STM32H7 Codex Pet

STM32H743 + 240x240 SPI IPS screen pet animation project.

## Hardware notes

LCD wiring:

- VCC -> 3V3
- GND -> GND
- SDA -> SDI / PB15
- SCL -> SCL / PB13
- DC  -> D/C / PB1
- BLK -> BLK / PB0
- RES -> SDO / PB14
- CS  -> not connected

Screen buttons:

- K1 -> PB3
- K2 -> PB5
- K3 -> PB7
- K4 -> PB9

SDMMC wiring:

- D0  -> PC8
- D1  -> PC9
- D2  -> PC10
- D3  -> PC11
- CLK/SCK -> PC12
- CMD -> PD2

## Asset format

Pet animation frames are stored on the SD card as RGB565 big-endian raw `.BIN` files under `/INNI/`.
Each frame is 192x208 pixels, 79,872 bytes.

Frame display position on 240x240 LCD:

- X = 24
- Y = 16

## Project

Open `USER/TIMER.uvprojx` in Keil MDK and build target `TIMER`.
