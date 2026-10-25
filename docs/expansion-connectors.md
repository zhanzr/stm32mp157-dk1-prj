# Expansion connectors — STM32MP157A-DK1 / DK2

Source: UM2534 Rev 1 (Tables 23, 24, 25). Applies to MB1272 (DK1/DK2).

## Arduino Uno V3 connectors

Four headers: CN16 (power), CN17 (analog A[0:5]), CN14 (digital D[0:7]),
CN13 (digital D[8:15]).

Operating voltage: 5 V, 3.3 V, and VDD supported for I/O compatibility.

**Caution (UM2534 7.16.2): do not supply 3.3 V or 5 V from the Arduino shield —
it can damage the kit.**

### CN16 — power

| Pin | Signal | STM32 pin | Comment |
|-----|--------|-----------|---------|
| 1   | NC     | -         | NC (reserved for test) |
| 2   | 3V3    | -         | IOREF 3V3 |
| 3   | NRST   | NRST      | NRST (reset input from shield, active low) |
| 4   | 3V3    | -         | 3V3 |
| 5   | 5V     | -         | 5V |
| 6   | GND    | -         | GND |
| 7   | GND    | -         | GND |
| 8   | VIN    | -         | Not connected |

### CN17 — analog

| Pin | Signal | STM32 pin  | Comment |
|-----|--------|------------|---------|
| 1   | A0     | PF14       | ADC1_IN0 |
| 2   | A1     | PF13       | ADC1_IN1 |
| 3   | A2     | ANA0       | ADC1_IN6 |
| 4   | A3     | ANA1       | ADC1_IN2 |
| 5   | A4     | PC3/PA12   | ADC1_IN13 (default PC3, SB24 ON; PA12 = I2C5_SDA via SB23 ON / SB24 OFF) |
| 6   | A5     | PF12/PA11  | ADC1_IN14 (default PF12, SB26 ON; PA11 = I2C5_SCL via SB25 ON / SB26 OFF) |

### CN14 — digital D[0:7]

| Pin | Signal | STM32 pin | Comment |
|-----|--------|-----------|---------|
| 1   | ARD_D0 | PE7       | USART7_RX |
| 2   | ARD_D1 | PE8       | USART7_TX |
| 3   | ARD_D2 | PE1       | IO |
| 4   | ARD_D3 | PD14      | TIM4_CH3 |
| 5   | ARD_D4 | PE10      | IO |
| 6   | ARD_D5 | PD15      | TIM4_CH4 |
| 7   | ARD_D6 | PE9       | TIM1_CH1 |
| 8   | ARD_D7 | PD1       | IO |

### CN13 — digital D[8:15]

| Pin | Signal  | STM32 pin | Comment |
|-----|---------|-----------|---------|
| 1   | ARD_D8  | PG3       | IO |
| 2   | ARD_D9  | PH6       | TIM12_CH1 |
| 3   | ARD_D10 | PE11      | SPI4_NSS and TIM1_CH2 |
| 4   | ARD_D11 | PE14      | SPI4_MOSI and TIM1_CH4 |
| 5   | ARD_D12 | PE13      | SPI4_MISO |
| 6   | ARD_D13 | PE12      | SPI4_SCK |
| 7   | GND     | -         | GND |
| 8   | VREFP   | -         | VREF+ |
| 9   | ARD_D14 | PA12      | I2C5_SDA (shared with ARD_A4) |
| 10  | ARD_D15 | PA11      | I2C5_SCL (shared with ARD_A5) |

Note: A4/A5 and D14/D15 are solder-bridge selectable — either ADC (PC3/PF12,
SB24/SB26 ON, default) or I2C5 (PA12/PA11, SB23/SB25 ON).

## CN2 — 40-pin GPIO expansion connector

Raspberry Pi shield-compatible header.

| Pin | Function / STM32 pin            | Pin | Function / STM32 pin           |
|-----|----------------------------------|-----|--------------------------------|
| 1   | 3V3                              | 2   | 5V                             |
| 3   | GPIO2 / I2C5_SDA (PA12)          | 4   | 5V                             |
| 5   | GPIO3 / I2C5_SCL (PA11)          | 6   | GND                            |
| 7   | GPIO4 / MCO1 (PA8)               | 8   | GPIO14 / USART3_TX (PB10)      |
| 9   | GND                              | 10  | GPIO15 / USART3_RX (PB12)      |
| 11  | GPIO17 / USART3_RTS (PG8)        | 12  | GPIO18 / SAI2_SCKA (PI5)       |
| 13  | GPIO27 / SDMMC3_D3 (PD7)         | 14  | GND                            |
| 15  | GPIO22 / SDMMC3_CK (PG15)        | 16  | GPIO23 / SDMMC3_CMD (PF1)      |
| 17  | 3V3                              | 18  | GPIO24 / SDMMC3_D0 (PF0)       |
| 19  | GPIO10 / SPI5_MOSI (PF9)         | 20  | GND                            |
| 21  | GPIO9 / SPI5_MISO (PF8)          | 22  | GPIO25 / SDMMC3_D1 (PF4)       |
| 23  | GPIO11 / SPI5_SCK (PF7)          | 24  | GPIO8 / SPI5_NSS (PF6)         |
| 25  | GND                              | 26  | GPIO7 (PF3)                    |
| 27  | I2C1_SDA (PF15)                  | 28  | I2C1_SCL (PD12)                |
| 29  | GPIO5 / MCO2 (PG2)               | 30  | GND                            |
| 31  | GPIO6 / TIM5_CH2 (PH11)          | 32  | GPIO12 / TIM4_CH2 (PD13)       |
| 33  | GPIO13 / TIM3_CH2 (PC7)          | 34  | GND                            |
| 35  | GPIO19 / SAI2_FSA (PI7)          | 36  | GPIO16 / USART3_CTS (PB13)     |
| 37  | GPIO26 / SDMMC3_D2 (PF5)         | 38  | GPIO20 / SAI2_SDA (PI6)        |
| 39  | GND                              | 40  | GPIO21 / SAI2_SDB (PF11)       |

Shared-peripheral notes (UM2534):

- SAI2 is shared between the audio codec and this connector (pins 12, 35, 38,
  40). By default the audio codec wins via solder bridges SB13/SB14/SB15/SB16.
- I2C1 (pins 27/28) is shared between the USB hub, DSI, and HDMI by default
  (not connected to the USB hub unless SB7/SB8 are closed).
- Other functions (I2C, UART, FDCAN...) can be remapped onto free pins via
  devicetree / STM32CubeMX.
