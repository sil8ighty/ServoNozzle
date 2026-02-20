# Wiring Guide - CNC Servo Sprayer

## Components Required

| Component | Description |
|-----------|-------------|
| ESP32-S3 DevKitC-1 | Main controller (USB-C) |
| ILI9341 2.8" TFT | 240x320 SPI display with XPT2046 touch |
| 2x Hobby Servo | SG90 (light duty) or MG996R (heavy duty) |
| MAX3232 Module | RS232 to TTL level converter |
| 5V 2A+ Power Supply | Dedicated servo power (do NOT use ESP32 power) |
| DB9 Cable/Connector | For CNC RS232 connection |
| Jumper Wires | Dupont or soldered connections |

---

## Pin Connections

### TFT Display (ILI9341 + XPT2046 Touch)

The display and touch controller share the same SPI bus with separate chip select pins.

| TFT Module Pin | ESP32-S3 GPIO | Notes |
|----------------|---------------|-------|
| VCC | 3V3 | 3.3V power |
| GND | GND | Ground |
| CS | **GPIO 10** | TFT chip select |
| RESET | **GPIO 8** | TFT reset |
| DC / RS | **GPIO 9** | Data/Command select |
| SDI / MOSI | **GPIO 11** | SPI data in (shared) |
| SCK | **GPIO 12** | SPI clock (shared) |
| LED / BL | **GPIO 7** | Backlight (HIGH = on) |
| SDO / MISO | **GPIO 13** | SPI data out (shared) |
| T_CLK | **GPIO 12** | Touch SPI clock (shared with SCK) |
| T_CS | **GPIO 6** | Touch chip select |
| T_DIN | **GPIO 11** | Touch MOSI (shared with SDI) |
| T_DO | **GPIO 13** | Touch MISO (shared with SDO) |
| T_IRQ | Not connected | Library uses polling |

### Servos

**IMPORTANT**: Servos must be powered from a separate 5V supply, NOT from the ESP32.
The servo signal wire connects to ESP32 GPIO. All grounds must be connected together.

| Servo | Wire | Connection |
|-------|------|------------|
| Servo 1 (Nozzle 1) | Signal (orange/white) | **GPIO 2** |
| Servo 1 | VCC (red) | External 5V supply (+) |
| Servo 1 | GND (brown/black) | External 5V supply (-) |
| Servo 2 (Nozzle 2) | Signal (orange/white) | **GPIO 21** |
| Servo 2 | VCC (red) | External 5V supply (+) |
| Servo 2 | GND (brown/black) | External 5V supply (-) |

```
                    +------------------+
  ESP32 GPIO2  ---->| Signal   Servo 1 |
  5V Supply +  ---->| VCC              |
  5V Supply -  ---->| GND              |
                    +------------------+

                    +------------------+
  ESP32 GPIO21 ---->| Signal   Servo 2 |
  5V Supply +  ---->| VCC              |
  5V Supply -  ---->| GND              |
                    +------------------+

  ESP32 GND --------+---- 5V Supply GND (COMMON GROUND!)
```

### CNC RS232 Connection (via MAX3232)

The CNC machine sends tool change commands over RS232. A MAX3232 module
converts RS232 voltage levels (+/-12V) to 3.3V TTL safe for the ESP32.

| MAX3232 Module Pin | Connection |
|--------------------|------------|
| VCC | ESP32 3V3 |
| GND | ESP32 GND |
| RX (TTL side, R1OUT) | **ESP32 GPIO 16** |
| TX (TTL side, T1IN) | Not connected (receive only) |
| RS232 RX (DB9 side) | CNC Machine TX (DB9 Pin 3) |
| RS232 GND (DB9 side) | CNC Machine GND (DB9 Pin 5) |

```
  CNC Machine                MAX3232 Module           ESP32-S3
  (DB9 Connector)            (Level Converter)
  +-------------+           +-----------------+      +----------+
  | Pin 3 (TX) -----RS232---| RS232 IN        |      |          |
  | Pin 5 (GND) ------GND---| GND        R1OUT|--TTL-| GPIO 16  |
  | Pin 2 (RX)  |           | VCC             |------|  3V3     |
  +-------------+           | GND             |------| GND      |
                            +-----------------+      +----------+
```

**DB9 RS232 Pinout Reference:**
- Pin 2 = RX (CNC receives) - not used in this project
- Pin 3 = TX (CNC sends) - **connect this to MAX3232**
- Pin 5 = GND - **connect to common ground**

---

## Complete Wiring Diagram (Text)

```
                            ESP32-S3 DevKitC-1
                           +----[USB-C]----+
                           |               |
                     3V3 --|               |-- GND
                           |               |
              TFT CS  GP10--|               |-- GP21 -- Servo 2 Signal
              TFT DC   GP9--|               |-- GP20
              TFT RST  GP8--|               |-- GP19
              TFT BL   GP7--|               |-- GP18
             Touch CS  GP6--|               |-- GP17
                           |               |-- GP16 -- MAX3232 R1OUT (CNC RX)
              SPI MOSI GP11--|               |-- GP15
              SPI SCLK GP12--|               |
              SPI MISO GP13--|               |
                           |               |
           Servo 1 Sig GP2--|               |
                           |               |
                           +---------------+
```

---

## Power Architecture

```
  +--------+          +------------+        +---+---+
  | USB-C  |---5V---->| ESP32-S3   |---3V3--| TFT   |
  | (PC or |          | (onboard   |        | Panel  |
  |  wall) |          |  regulator)|---3V3--| MAX3232|
  +--------+          +------+-----+        +-------+
                             |
                            GND (COMMON)
                             |
  +-----------+              |
  | 5V 2A+   |---5V---------+--- Servo 1 VCC
  | Supply    |              +--- Servo 2 VCC
  |           |---GND--------+--- Servo 1 GND
  +-----------+              +--- Servo 2 GND
                             +--- ESP32 GND (common!)
```

**Key power notes:**
1. ESP32-S3 DevKitC is powered via USB-C (5V, regulated to 3.3V onboard)
2. Servos MUST use a separate 5V 2A+ power supply
3. All grounds must be connected together (ESP32, servo supply, MAX3232)
4. Never power servos from the ESP32 3.3V or 5V pin (insufficient current, causes brownouts)
5. If using MG996R servos, use a 5V 3A+ supply (they draw ~1A under load each)

---

## Touch Calibration

The touch controller raw values (0-4095) need mapping to screen pixels.
Default calibration in the firmware: `map(raw, 200, 3800, 0, 239/319)`.

If touch is inaccurate, enable the debug output in `ui.cpp` (uncomment the
Serial.printf in `touchRead`) and note the raw X/Y values at each corner
of the screen, then adjust the `200` and `3800` values accordingly.

---

## Serial Protocol

The system listens for CNC G-code tool change commands:

| Command | Example | Action |
|---------|---------|--------|
| `Tnn` | `T1`, `T05`, `T12` | Switch to tool number, load saved positions |

The parser detects the letter `T` followed by 1-2 digits (T0 through T99).
Common CNC formats like `T1 M06`, `T01`, `N100 T5 M6` are all handled.

Baud rate: **38400**, 8N1 (8 data bits, no parity, 1 stop bit).
