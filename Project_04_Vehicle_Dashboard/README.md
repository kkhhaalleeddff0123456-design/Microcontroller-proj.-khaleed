# Project 04 — Vehicle Dashboard

> Part of the **Embedded Systems Projects Book** — see the
> [book README](../README.md) for the shared platform baseline, layer rules and
> common rubric. Everything in this file is *in addition to* those rules.

---

## 1. Project Identity

| Field | Value |
|-------|-------|
| **Project code** | `PRJ-04-DASHBOARD` |
| **Team size** | 2 – 3 students |
| **Build window** | Days 11 – 15 (Jul 26 – Jul 30, 2026) |
| **Demo & submission** | July 30, 2026 |
| **Dominant skill** | Timer1 input capture, shared SPI bus, non-volatile odometer |
| **MCU** | ATmega32A @ 8 MHz |
| **Simulator** | SimulIDE 1.x |

---

## 2. Description

The Vehicle Dashboard is a digital instrument cluster: the piece of firmware that
tells the driver how fast the car is going, how much fuel is left, and whether
anything is about to break.

Two of its measurements come from **pulse trains**, not from potentiometers:

- **Road speed** is derived from a wheel-speed sensor whose *period* is measured
  with **Timer1 Input Capture** on `ICP1`. Period measurement (not frequency
  counting) is what gives usable resolution at low speed.
- **Engine RPM** is derived from ignition pulses *counted* on `INT0` over a fixed
  250 ms window. Counting is the right technique here because RPM is high and
  changes slowly.

You will implement both and must be able to explain, in the defence, why each
signal uses the technique it does.

Four analog channels (fuel, coolant temperature, battery voltage, oil pressure)
feed a **warning-lamp matrix** driven through a **74HC595 shift register that
shares the SPI bus with the EEPROM**. Managing two chip selects on one bus is
the second core lesson.

The **odometer** must survive every power cut without ever losing more than
100 m — a real constraint that forces you to think about write throttling,
wear levelling and atomic updates.

---

## 3. Objectives

1. Measure a pulse period with Timer1 Input Capture, including 16-bit overflow
   extension for low speeds.
2. Count pulses on an external interrupt over a fixed window and convert to RPM.
3. Share one SPI bus between two devices with independent chip selects, without
   corrupting either.
4. Drive an 8-lamp cluster through a 74HC595 shift register.
5. Implement a bulb-check sequence and a lamp priority scheme.
6. Persist an odometer with bounded loss and no torn writes.
7. Model an ignition key as a state machine with a cranking phase and a
   limp-home fault path.

---

## 4. Learning Outcomes

| ID | Outcome |
|----|---------|
| LO-1 | Configure Timer1 Input Capture (`ICES1`, `ICNC1`, `ICR1`) and read `ICR1` inside `ISR(TIMER1_CAPT_vect)` |
| LO-2 | Extend a 16-bit capture to 32 bits using an overflow counter, and explain the race between `TOV1` and a capture |
| LO-3 | Choose between period measurement and frequency counting from the signal's frequency range |
| LO-4 | Sequence two SPI slaves safely: deassert one CS before asserting the other, and restore SPI mode if they differ |
| LO-5 | Shift 8 bits out to a 74HC595 over hardware SPI and latch them with a GPIO strobe |
| LO-6 | Design an EEPROM update policy that bounds data loss to a stated maximum |
| LO-7 | Detect a stalled pulse train (zero speed) with a timeout rather than waiting forever |

---

## 5. Estimated Duration

| Phase | Hours | Course day |
|-------|:-----:|-----------|
| Requirements analysis & pin freeze | 3 | Day 11 |
| Architecture, FSM, lamp-priority design | 4 | Day 11 |
| Analog channels, 595 cluster, LCD | 6 | Day 12 |
| Input capture + RPM counting + tick | 8 | Day 13 |
| Odometer, EEPROM policy, UART | 6 | Day 14 |
| Testing & debugging | 5 | Day 15 |
| Documentation, report, video | 4 | Day 15 + evening |
| **Total** | **36 h** | |

---

## 6. Hardware Components

| # | Component | Qty | SimulIDE part | Purpose |
|---|-----------|:---:|---------------|---------|
| 1 | ATmega32A | 1 | `atmega32` | Controller |
| 2 | Potentiometer 10 kΩ | 4 | `Potentiometer` | Fuel, coolant, battery, oil pressure |
| 3 | Variable clock / function generator | 1 | `Clock` | Wheel-speed pulses (0.5 – 500 Hz) |
| 4 | Variable clock / function generator | 1 | `Clock` | Ignition tach pulses (0 – 300 Hz) |
| 5 | 74HC595 shift register | 1 | `Shift Reg.` | 8 warning lamps |
| 6 | LED + 330 Ω | 8 | `Led` | Warning-lamp cluster |
| 7 | Switch (SPST) | 6 | `Switch` | Turn L/R, high beam, handbrake, seatbelt, door |
| 8 | Push button | 3 | `Push` | Ignition key, Start, Display-cycle |
| 9 | Buzzer | 1 | `Buzzer` | Over-speed / critical chime |
| 10 | 16×2 LCD + PCF8574 | 1 | `Lcd` + `I2CToParallel` | Main display |
| 11 | 25LC256 SPI EEPROM | 1 | `Memory (SPI)` | Odometer, trip, config |
| 12 | Serial terminal | 1 | `SerialPort` | Diagnostics port |

---

## 7. Pin Map

| Signal | Pin | Port bit | Direction | Notes |
|--------|-----|----------|-----------|-------|
| Fuel level | 40 | `PA0` / ADC0 | Analog in | 0 – 1023 → 0 – 100 % |
| Coolant temperature | 39 | `PA1` / ADC1 | Analog in | 0 – 1023 → 40 – 130 °C |
| Battery voltage | 38 | `PA2` / ADC2 | Analog in | 0 – 1023 → 0.0 – 16.0 V |
| Oil pressure | 37 | `PA3` / ADC3 | Analog in | 0 – 1023 → 0.0 – 10.0 bar |
| Turn-left switch | 1 | `PB0` | In, pull-up | Active low |
| Turn-right switch | 2 | `PB1` | In, pull-up | Active low |
| High-beam switch | 3 | `PB2` | In, pull-up | Active low |
| Trip-reset button | 4 | `PB3` | In, pull-up | Hold 2 s to reset trip |
| SPI `SS` — EEPROM | 5 | `PB4` | Out | Active low |
| SPI `MOSI` | 6 | `PB5` | Out | Shared: EEPROM + 74HC595 |
| SPI `MISO` | 7 | `PB6` | In | EEPROM only |
| SPI `SCK` | 8 | `PB7` | Out | Shared |
| I2C `SCL` | 22 | `PC0` | Out | 4.7 kΩ pull-up |
| I2C `SDA` | 23 | `PC1` | Bidir | 4.7 kΩ pull-up |
| 74HC595 `RCLK` (latch) | 24 | `PC2` | Out | Rising edge latches |
| Handbrake switch | 25 | `PC3` | In, pull-up | Active low = engaged |
| Seatbelt switch | 26 | `PC4` | In, pull-up | Active low = unbuckled |
| Door switch | 27 | `PC5` | In, pull-up | Active low = open |
| CPU-load test pin | 28 | `PC6` | Out | Timing measurement |
| LCD backlight | 29 | `PC7` | Out | Bonus dimming |
| USART `RXD` | 14 | `PD0` | In | 9600 8N1 |
| USART `TXD` | 15 | `PD1` | Out | 9600 8N1 |
| Tach pulse input | 16 | `PD2` / INT0 | In | Rising edge, counted |
| Ignition key | 17 | `PD3` / INT1 | In, pull-up | Falling edge |
| Start button | 18 | `PD4` | In, pull-up | Polled |
| Display-cycle button | 19 | `PD5` | In, pull-up | Polled |
| Wheel-speed pulse | 20 | `PD6` / `ICP1` | In | **Timer1 Input Capture** |
| Buzzer | 21 | `PD7` / OC2 | Out | Chime tones |

> `PC2` – `PC5` carry the 595 latch and three body switches: **clear the
> `JTAGEN` fuse** or the lamp cluster will never latch.

### 74HC595 lamp map

| Q output | Lamp | Colour | Priority |
|:--------:|------|--------|:--------:|
| `Q0` | Low fuel | Amber | 5 |
| `Q1` | Oil pressure | Red | 1 |
| `Q2` | Battery / charging | Red | 2 |
| `Q3` | Coolant temperature | Red | 3 |
| `Q4` | Check engine | Amber | 4 |
| `Q5` | Left turn | Green | — |
| `Q6` | Right turn | Green | — |
| `Q7` | High beam | Blue | — |

---

## 8. Peripherals Used

| Peripheral | Configuration | Role |
|------------|---------------|------|
| **GPIO** | Switches in + pull-up; `PC2`, `PC6`, `PC7` out | Body inputs, latch, test pins |
| **ADC** | Single conversion, prescaler 64, AVCC ref | 4 sensor channels |
| **Timer0** | CTC, prescaler 1024, `OCR0 = 77` | 10 ms system tick |
| **Timer1** | Normal mode, prescaler 64, `ICIE1` + `TOIE1`, noise canceller on, rising edge | Wheel-speed period capture |
| **Timer2** | Fast PWM, OC2 | Buzzer / chime tones |
| **INT0** | Rising edge | Tach pulse counting |
| **INT1** | Falling edge | Ignition key |
| **USART** | 9600 8N1, RX interrupt | Diagnostics port |
| **SPI** | Master, Mode 0, f/16 | EEPROM (`PB4`) **and** 74HC595 (`PC2` latch) |
| **I2C (TWI)** | Master, 100 kHz | PCF8574 → LCD |

### Speed maths — derive this in your report

```
Wheel sensor      : 4 pulses per wheel revolution
Wheel circumference: 2.000 m
Distance per pulse : 500 mm

Timer1 prescaler 64 @ 8 MHz  →  125 kHz  →  1 tick = 8 µs

period_us  = capture_delta_ticks × 8
speed_kmh  = 1 800 000 / period_us          (see derivation below)

  500 mm / period_us  =  (500e-3 m) / (period_us × 1e-6 s)
                      =  500 000 / period_us  m/s
                      =  1 800 000 / period_us  km/h     (× 3.6)
```

| Speed | `period_us` | Timer1 ticks | Fits 16-bit? |
|------:|------------:|-------------:|:------------:|
| 200 km/h | 9 000 | 1 125 | ✔ |
| 60 km/h | 30 000 | 3 750 | ✔ |
| 10 km/h | 180 000 | 22 500 | ✔ |
| 3.5 km/h | 514 000 | 64 300 | ✔ (just) |
| 2 km/h | 900 000 | 112 500 | ✘ — **needs overflow extension** |

Below ≈ 3.4 km/h a single 16-bit capture wraps. You **must** extend the capture
to 32 bits with a `TIMER1_OVF_vect` counter, and handle the classic race: if
`TOV1` is pending *and* the captured value is small, the overflow happened
before the capture.

### RPM maths

```
4-cylinder 4-stroke → 2 ignition pulses per crankshaft revolution
count pulses over a 250 ms window
RPM = count × (1000/250) × 60 / 2 = count × 120
```

At 6000 RPM the window collects 50 pulses — comfortably inside a `uint8_t`, but
use `uint16_t` anyway and say why.

---

## 9. Software Architecture

### 9.1 Layer view

```
┌───────────────────────────────────────────────────────────────────┐
│ APP                                                               │
│  ┌──────────┐ ┌──────────┐ ┌─────────┐ ┌────────┐ ┌────────────┐  │
│  │ cluster  │ │ warnings │ │ trip &  │ │ display│ │  console   │  │
│  │  _fsm    │ │ (priority)│ │  odo    │ │ pages  │ │            │  │
│  └────┬─────┘ └────┬─────┘ └────┬────┘ └───┬────┘ └─────┬──────┘  │
│       └────────────┴──── scheduler (10 ms) ┴────────────┘         │
├───────────────────────────────────────────────────────────────────┤
│ HAL                                                               │
│  speedo.c  tacho.c  gauges.c  lamps595.c  lcd_i2c.c               │
│  eeprom_spi.c  bodysw.c  chime.c                                  │
├───────────────────────────────────────────────────────────────────┤
│ MCAL                                                              │
│  dio.c  adc.c  timer.c  icu.c  exti.c  usart.c  spi.c  i2c.c      │
├───────────────────────────────────────────────────────────────────┤
│ LIB    STD_TYPES.h  BIT_MATH.h  ring_buffer.c                     │
└───────────────────────────────────────────────────────────────────┘
```

### 9.2 The shared SPI bus — the tricky part

`lamps595.c` and `eeprom_spi.c` both use `MCAL/spi.c`. The bus is not free-for-all:

```c
/* Every SPI transaction is bracketed. No exceptions. */
void SPI_Acquire(SpiSlave_t s);   /* deassert all CS, apply this slave's mode,
                                     then assert its CS                      */
void SPI_Release(void);           /* deassert CS, leave the bus idle          */
```

Rules that earn (or lose) the SPI marks:

1. **Never** assert a second CS while another is low.
2. The 74HC595 has no `MISO`; do not read `SPDR` expecting data from it.
3. The 595 latch (`PC2`) is pulsed **after** `SPI_Release()`, never between
   bytes.
4. An EEPROM write is a multi-step sequence spread over several ticks — the
   lamp task must not steal the bus in the middle of it. Use a simple
   `busOwner` guard and make the lamp update wait a tick.
5. Both devices use Mode 0 here, so no mode switch is needed — say so
   explicitly in your report rather than leaving it implicit.

### 9.3 Warning-lamp priority

More than one warning can be active at once, but the LCD banner shows only one.
`warnings.c` resolves it:

```c
/* Lowest number wins. Returns WARN_NONE if nothing is active. */
Warn_t WRN_Highest(const CarData_t *d);
```

| Priority | Warning | Condition | Class |
|:--------:|---------|-----------|-------|
| 1 | `WARN_OIL` | Oil pressure < 1.0 bar with engine running | **Critical** → limp home |
| 2 | `WARN_BATT` | Battery < 12.0 V with engine running, or > 15.0 V | Critical |
| 3 | `WARN_COOLANT` | Coolant > 110 °C | **Critical** → limp home |
| 4 | `WARN_CHECK` | Any sensor implausible | Warning |
| 5 | `WARN_FUEL` | Fuel < 10 % | Warning |
| 6 | `WARN_OVERSPEED` | Speed > limit | Warning |
| 7 | `WARN_SEATBELT` | Seatbelt unbuckled ∧ speed > 10 km/h | Info |
| 8 | `WARN_DOOR` | Door open ∧ speed > 5 km/h | Info |
| 9 | `WARN_HANDBRAKE` | Handbrake on ∧ speed > 5 km/h | Info |

### 9.4 Module responsibilities

| Module | Owns | Public API (suggested) |
|--------|------|------------------------|
| `cluster_fsm` | Ignition state, cranking, limp home | `FSM_Init`, `FSM_Run`, `FSM_GetState` |
| `warnings` | Priority resolution, latching | `WRN_Update`, `WRN_Highest`, `WRN_Mask` |
| `speedo` | Capture delta → km/h, stall timeout | `SPD_OnCapture`, `SPD_OnOverflow`, `SPD_GetKmh` |
| `tacho` | INT0 count → RPM | `TAC_OnPulse`, `TAC_Update250ms`, `TAC_GetRpm` |
| `odometer` | Distance integration + persistence policy | `ODO_AddDistance`, `ODO_GetTotal`, `ODO_GetTrip`, `ODO_ResetTrip` |
| `gauges` | ADC scaling + plausibility | `GAU_Update`, `GAU_Fuel`, `GAU_CoolantC`, `GAU_BattmV`, `GAU_OilBarX10` |
| `lamps595` | Cluster byte + blink phases + bulb check | `LMP_Set`, `LMP_Refresh`, `LMP_BulbCheck` |
| `display` | Page cycling, formatting | `DSP_Next`, `DSP_Render` |
| `chime` | Tone patterns | `CHM_Play(pattern)` |

### 9.5 Concurrency contract

- `ISR(TIMER1_CAPT_vect)` reads `ICR1`, combines it with the overflow count,
  stores a 32-bit delta into a `volatile` slot, sets a flag. **No division in
  the ISR** — the km/h conversion happens in the 100 ms task.
- `ISR(TIMER1_OVF_vect)` increments a `volatile uint16_t` and nothing else.
- `ISR(INT0_vect)` increments a `volatile uint16_t` pulse counter and nothing
  else.
- 32-bit shared values (`odoMetres`, capture delta) are read under
  `ATOMIC_BLOCK`.

---

## 10. Data Dictionary (required data)

### 10.1 Runtime data — `DD-01 CarData_t`

```c
typedef struct {
    uint16_t speedKmh;         /* 0..250                                 */
    uint16_t rpm;              /* 0..8000                                */
    uint8_t  fuelPct;          /* 0..100                                 */
    int16_t  coolantC;         /* -40..130                               */
    uint16_t battmV;           /* 0..16000                               */
    uint8_t  oilBarX10;        /* 0..100  (0.0..10.0 bar)                */
    uint32_t odoMetres;        /* lifetime, metres                       */
    uint32_t tripMetres;       /* since last reset                       */
    uint16_t maxSpeedKmh;      /* session record                         */
    uint16_t avgSpeedKmh;      /* trip average                           */
    uint16_t warnMask;         /* bit per Warn_t, 1 = active             */
    uint8_t  lampByte;         /* what was last shifted to the 595       */
    uint8_t  turnLeft   : 1;
    uint8_t  turnRight  : 1;
    uint8_t  highBeam   : 1;
    uint8_t  handbrake  : 1;
    uint8_t  seatbelt   : 1;   /* 1 = unbuckled                          */
    uint8_t  doorOpen   : 1;
    uint8_t  engineRun  : 1;
    uint8_t  limpHome   : 1;
    uint8_t  state;            /* ClusterState_t                         */
    uint8_t  page;             /* DisplayPage_t                          */
    uint32_t ignitionSec;      /* seconds since key on                   */
} CarData_t;
```

### 10.2 Persisted record — `DD-02 DashCfg_t`

```c
#define DSH_MAGIC   0x4443u      /* 'D','C'                              */
#define DSH_VERSION 0x01u

typedef struct {
    uint16_t magic;
    uint8_t  version;
    uint32_t odoMetres;          /* lifetime odometer                    */
    uint32_t tripMetres;         /* trip meter                           */
    uint16_t maxSpeedRecord;     /* all-time                             */
    uint16_t speedLimitKmh;      /* over-speed warning  (default 120)    */
    uint8_t  fuelWarnPct;        /* (default 10)                         */
    uint8_t  coolantWarnC;       /* (default 110)                        */
    uint8_t  oilWarnBarX10;      /* (default 10 = 1.0 bar)               */
    uint16_t battLowmV;          /* (default 12000)                      */
    uint16_t battHighmV;         /* (default 15000)                      */
    uint8_t  pulsesPerRev;       /* wheel sensor      (default 4)        */
    uint16_t wheelCircMm;        /* (default 2000)                       */
    uint8_t  tachPulsesPerRev;   /* (default 2)                          */
    uint16_t ignitionCycles;
    uint8_t  writeSlot;          /* wear-levelling slot 0..7             */
    uint8_t  checksum;
} DashCfg_t;                     /* 33 bytes                             */
```

### 10.3 Enumerations — `DD-03`

```c
typedef enum { CS_OFF = 0, CS_ACC, CS_IGNITION, CS_BULBCHECK,
               CS_CRANKING, CS_RUNNING, CS_LIMP_HOME,
               CS_STALLED }                                ClusterState_t;

typedef enum { WARN_NONE = 0, WARN_OIL, WARN_BATT, WARN_COOLANT,
               WARN_CHECK, WARN_FUEL, WARN_OVERSPEED,
               WARN_SEATBELT, WARN_DOOR, WARN_HANDBRAKE }  Warn_t;

typedef enum { PG_MAIN = 0, PG_TRIP, PG_ENGINE, PG_ELECTRICAL,
               PG_DIAG }                                   DisplayPage_t;

typedef enum { SPI_SLAVE_EEPROM = 0, SPI_SLAVE_LAMPS }     SpiSlave_t;
```

### 10.4 Capture record — `DD-04`

```c
typedef struct {
    volatile uint16_t lastIcr;      /* previous ICR1                     */
    volatile uint16_t ovfCount;     /* Timer1 overflows since last edge  */
    volatile uint32_t deltaTicks;   /* 32-bit period, ready for maths    */
    volatile uint8_t  fresh;        /* set by ISR, cleared by the task   */
    uint16_t          stallTicks;   /* 10 ms units since last edge       */
} Capture_t;
```

### 10.5 Derived constants — `DD-05`

| Constant | Value | Meaning |
|----------|-------|---------|
| `TIMER1_TICK_US` | 8 | Prescaler 64 @ 8 MHz |
| `SPEED_NUM` | 1 800 000 | `speed_kmh = SPEED_NUM / period_us` |
| `MM_PER_PULSE` | 500 | `wheelCircMm / pulsesPerRev` |
| `RPM_FACTOR` | 120 | `rpm = count250ms × 120` |
| `SPEED_STALL_TICKS` | 100 | 1 s without an edge → speed = 0 |
| `BULBCHECK_TICKS` | 300 | 3 s lamp test |
| `CRANK_MAX_TICKS` | 500 | 5 s cranking limit |
| `TURN_BLINK_TICKS` | 45 | 450 ms on / 450 ms off (≈ 67 flashes/min) |
| `ODO_SAVE_METRES` | 100 | Persist every 100 m travelled |
| `ODO_SAVE_SEC` | 60 | …or every 60 s, whichever comes first |
| `TRIP_RESET_TICKS` | 200 | 2 s button hold |

---

## 11. System Specifications

### 11.1 Speed

| Band | Range | Behaviour |
|------|-------|-----------|
| Stationary | 0 km/h | No edge for 1 s |
| Low | 1 – 30 km/h | Requires 32-bit capture extension |
| Normal | 31 – 120 km/h | |
| Over limit | > `speedLimitKmh` (120) | `WARN_OVERSPEED`, chime every 5 s |
| Implausible | > 250 km/h | Rejected as noise, `WARN_CHECK` |

### 11.2 Engine RPM

| Band | Range | Meaning |
|------|-------|---------|
| Stopped | 0 | Engine not running |
| Cranking | 100 – 400 | Starter engaged |
| Idle | 600 – 1000 | Running |
| Normal | 1001 – 5500 | |
| Red line | > 5500 | Chime + `WARN_CHECK` |

Engine is declared **running** when RPM > 500 for 500 ms, and **stalled** when
RPM < 300 for 1 s while in `CS_RUNNING`.

### 11.3 Analog channel scaling

| Channel | ADC range | Engineering range | Formula |
|---------|-----------|-------------------|---------|
| Fuel | 0 – 1023 | 0 – 100 % | `raw*100/1023` |
| Coolant | 0 – 1023 | −40 – 130 °C | `(raw*170/1023) - 40` |
| Battery | 0 – 1023 | 0 – 16000 mV | `raw*16000/1023` |
| Oil pressure | 0 – 1023 | 0 – 100 (0.0 – 10.0 bar) | `raw*100/1023` |

All intermediates must be `uint32_t` — `raw*16000` reaches 16 368 000.

### 11.4 Warning thresholds

| Warning | Trip | Clear | Latching |
|---------|------|-------|:--------:|
| Low fuel | < 10 % | > 13 % | No |
| Oil pressure | < 1.0 bar, engine running, 2 s | > 1.5 bar | **Yes** |
| Battery low | < 12.0 V, engine running, 5 s | > 12.5 V | No |
| Battery high | > 15.0 V, 5 s | < 14.5 V | No |
| Coolant | > 110 °C, 3 s | < 105 °C | **Yes** |
| Over-speed | > limit | < limit − 5 | No |
| Seatbelt | unbuckled ∧ > 10 km/h | buckled ∨ < 10 km/h | No |
| Door | open ∧ > 5 km/h | closed | No |
| Handbrake | engaged ∧ > 5 km/h | released | No |

Latching warnings clear only at the next key-off/key-on cycle.

### 11.5 Fuel-tank model

Tank capacity 50 L. `fuelPct` maps linearly. Range remaining is computed from
the trip average consumption; if no consumption data exists yet, display `--`.

---

## 12. Inputs & Outputs

### 12.1 Inputs

| ID | Name | Channel | Type | Sample rate |
|----|------|---------|------|-------------|
| IN-1 | Fuel level | ADC0 | Analog | 2 Hz (heavily damped) |
| IN-2 | Coolant temperature | ADC1 | Analog | 2 Hz |
| IN-3 | Battery voltage | ADC2 | Analog | 10 Hz |
| IN-4 | Oil pressure | ADC3 | Analog | 10 Hz |
| IN-5 | Wheel-speed pulses | `PD6`/`ICP1` | Pulse period | Every edge |
| IN-6 | Tach pulses | `PD2`/INT0 | Pulse count | Window 250 ms |
| IN-7 | Ignition key | `PD3`/INT1 | Digital, edge | Interrupt |
| IN-8 | Start button | `PD4` | Digital, polled | 100 Hz |
| IN-9 | Display cycle | `PD5` | Digital, polled | 100 Hz |
| IN-10 | Turn L / R, high beam | `PB0`…`PB2` | Digital, polled | 50 Hz |
| IN-11 | Trip reset | `PB3` | Digital, polled | 100 Hz |
| IN-12 | Handbrake, seatbelt, door | `PC3`…`PC5` | Digital, polled | 20 Hz |
| IN-13 | Console | USART RX | ASCII line | Interrupt |

### 12.2 Outputs

| ID | Name | Pin | Type | Meaning |
|----|------|-----|------|---------|
| OUT-1 | Lamp cluster | SPI + `PC2` | 8-bit shift register | Warning lamps |
| OUT-2 | LCD | I2C | 16×2 text | Five display pages |
| OUT-3 | Buzzer | `PD7`/OC2 | PWM tone | Chimes |
| OUT-4 | LCD backlight | `PC7` | Digital / PWM | Dimming (bonus) |
| OUT-5 | Diagnostics | USART TX | ASCII | 5 s frame + events |

---

## 13. Functional Requirements

### FR-01 — Road-speed measurement by input capture

The system **shall** measure the wheel-sensor period with Timer1 Input Capture
and publish road speed in km/h at **10 Hz**.

**Acceptance criteria**
- Rising-edge capture with the input noise canceller (`ICNC1`) enabled.
- The captured period is extended to 32 bits with a Timer1 overflow counter, so
  speeds down to **1 km/h** are measured.
- Accuracy ±1 km/h between 5 and 200 km/h against the injected frequency.
- The ISR performs **no division**; conversion happens in the 100 ms task.
- Resolution at 100 km/h is better than 1 km/h — show the arithmetic.

### FR-02 — Zero-speed detection

If no capture edge arrives for **1 s**, the system **shall** publish `speed = 0`.

**Acceptance criteria**
- Stopping the pulse source makes the display read 0 within 1.1 s.
- No stale speed is ever displayed after the pulses stop.
- Restarting the pulses produces a valid speed within two edges (the first
  post-stall delta is discarded).

### FR-03 — Engine RPM measurement

The system **shall** count `INT0` pulses over a **250 ms** window and publish RPM
at 4 Hz.

**Acceptance criteria**
- `RPM = count × 120` for the default 2 pulses/revolution.
- Accuracy ±60 RPM from 600 to 6000 RPM.
- The counter is read and zeroed atomically at the window boundary — no pulse is
  double-counted or lost.

### FR-04 — Analog gauge acquisition

The system **shall** sample all four analog channels with the rates and scaling
of §11.3.

**Acceptance criteria**
- Fuel and coolant are damped with an 8-sample moving average — fuel slosh must
  not make the gauge jump.
- Battery and oil are lightly filtered (median-of-3) so a genuine drop is caught
  quickly.
- All intermediates are `uint32_t`; verified by inspecting the scaling code.

### FR-05 — Odometer integration

The system **shall** accumulate distance in **metres** from wheel pulses.

**Acceptance criteria**
- Each captured edge adds `MM_PER_PULSE` to a millimetre accumulator; whole
  metres are carried into `odoMetres` and `tripMetres`.
- Integration is exact — no drift from repeated km/h × time multiplication.
- 1000 pulses at 500 mm each register exactly 500 m.
- `odoMetres` is `uint32_t` and saturates rather than wrapping.

### FR-06 — Odometer persistence policy

The system **shall** persist the odometer such that **no more than 100 m** is
ever lost to a power cut.

**Acceptance criteria**
- A write occurs every `ODO_SAVE_METRES` (100 m) travelled **or** every
  `ODO_SAVE_SEC` (60 s), whichever comes first, and immediately on key-off.
- Writes use the 8-slot wear-levelling scheme of §19.2; the slot with the highest
  valid sequence number wins at boot.
- Yanking power mid-drive and rebooting shows a value within 100 m of the truth.
- A write interrupted halfway must **not** produce a corrupt odometer — the
  previous slot remains valid. Demonstrate this in TC-19.

### FR-07 — Trip meter

The system **shall** maintain a resettable trip meter with trip distance,
average speed and elapsed time.

**Acceptance criteria**
- Holding the trip-reset button for **2 s** zeroes trip distance, trip time and
  average; the lifetime odometer is untouched.
- Average speed = trip metres × 3.6 / trip seconds, integer maths.
- A short press cycles the display page instead of resetting — the hold is what
  resets.

### FR-08 — Warning-lamp cluster over 74HC595

The system **shall** drive all eight cluster lamps by shifting one byte out over
SPI and pulsing `PC2`.

**Acceptance criteria**
- The lamp byte is refreshed every **50 ms**.
- The 595 is written **only** through `SPI_Acquire(SPI_SLAVE_LAMPS)` /
  `SPI_Release()`.
- The latch pulse occurs after the byte is fully shifted, and is ≥ 1 µs wide.
- No lamp flicker at the 50 ms refresh rate.

### FR-09 — Bulb check

On entering `CS_IGNITION` the system **shall** illuminate **all eight** lamps for
**3 s**, then extinguish those with no active condition.

**Acceptance criteria**
- Exactly 3 s ±100 ms, timed by the scheduler.
- The LCD shows `BULB CHECK` during the sequence.
- After the check, the lamp byte reflects only genuinely active warnings.

### FR-10 — Warning priority and banner

The system **shall** resolve simultaneously active warnings by the priority table
of §9.3 and show only the highest on the LCD banner.

**Acceptance criteria**
- With oil and fuel warnings both active, the banner shows the oil warning; both
  lamps are lit.
- `warnMask` in the telemetry frame reports every active warning simultaneously.
- Critical warnings (priority 1 – 3) latch until the next key cycle.

### FR-11 — Turn indicators

Turn-signal switches **shall** blink their lamps at **450 ms on / 450 ms off**
with a synchronised buzzer tick.

**Acceptance criteria**
- Both switches on simultaneously → hazard behaviour: both lamps blink in phase.
- Blink phase is generated by the scheduler, not by a delay loop.
- Releasing the switch extinguishes the lamp within one blink period.

### FR-12 — Ignition state machine

The system **shall** implement the ignition sequence
`OFF → ACC → IGNITION → BULBCHECK → CRANKING → RUNNING`.

**Acceptance criteria**
- Each ignition-key press advances `OFF → ACC → IGNITION`; a long press (2 s)
  returns to `OFF`.
- The Start button is honoured only in `CS_IGNITION` after the bulb check.
- Cranking is limited to `CRANK_MAX_TICKS` (5 s); on expiry it returns to
  `CS_IGNITION` and logs `!EVT,CRANK,FAIL`.
- `CS_RUNNING` is entered when RPM > 500 for 500 ms.
- In `CS_OFF` the cluster is dark, the LCD shows only the odometer for 10 s,
  then blanks.

### FR-13 — Stall detection

If RPM falls below 300 for **1 s** while in `CS_RUNNING`, the system **shall**
enter `CS_STALLED`.

**Acceptance criteria**
- Lamp cluster shows oil + battery lamps (as a real car does with the engine
  off but the key on).
- `!EVT,ENGINE,STALL` is logged.
- Pressing Start re-enters `CS_CRANKING`.

### FR-14 — Limp-home mode

A **critical** warning (oil pressure or coolant) **shall** put the cluster into
`CS_LIMP_HOME`.

**Acceptance criteria**
- Continuous chime for 5 s, then one chirp every 10 s.
- LCD banner alternates `!! STOP ENGINE !!` with the cause every 1 s.
- The state is latched for the rest of the key cycle even if the sensor recovers.
- `!EVT,LIMP,OIL` (or `COOLANT`) is logged with the sensor snapshot.

### FR-15 — Over-speed warning

Exceeding `speedLimitKmh` **shall** raise `WARN_OVERSPEED`.

**Acceptance criteria**
- Chime once, then repeat every 5 s while over the limit.
- Clears when speed falls 5 km/h below the limit (hysteresis).
- The limit is settable 30 – 200 km/h and persists in EEPROM.

### FR-16 — Maximum-speed record

The system **shall** record the highest speed of the current key cycle and the
all-time maximum.

**Acceptance criteria**
- Session max resets at key-on; all-time max persists in EEPROM.
- A new all-time record triggers `!EVT,MAXSPEED,<kmh>`.
- Implausible readings (> 250 km/h) never enter the record.

### FR-17 — Display pages

The Display-cycle button **shall** rotate through five LCD pages:

```
PG_MAIN        SPD:  62 km/h    | RPM:2450 F:78%
PG_TRIP        TRIP: 42.7 km    | AVG:58 T:00:44
PG_ENGINE      RPM:2450 C: 92C  | OIL:3.4bar
PG_ELECTRICAL  BATT:14.2V       | ODO:123456 km
PG_DIAG        WARN:0x0021      | ST:RUN UP:2650
```

**Acceptance criteria**
- Refresh 250 ms, changed characters only, no flicker.
- The page selection is remembered across key cycles (persisted with the config).
- In `CS_LIMP_HOME` the banner overrides the page every other second.

### FR-18 — Sensor plausibility

Any analog channel pinned at 0 or 1023 for **5 s** **shall** raise `WARN_CHECK`
and name the channel in the diagnostics stream.

**Acceptance criteria**
- The affected gauge shows `---` rather than a false value.
- The check-engine lamp lights.
- The warning clears 2 s after the reading returns to range.

### FR-19 — Diagnostics telemetry and console

The system **shall** transmit the frame of §18.1 every **5 s** and accept the
commands of §18.2, with the book's parser robustness rules.

**Acceptance criteria**
- The odometer **cannot** be increased from the console — `SET ODO` is rejected
  with `ERR PROTECTED`. Say why in your report.
- `SET ODO` **may** be allowed once, guarded by `UNLOCK <code>`, as a bonus.

---

## 14. Non-Functional Requirements

| ID | Requirement |
|----|-------------|
| **NFR-01** | Compiles with `avr-gcc -std=c99 -Wall -Wextra -Os`, zero warnings. |
| **NFR-02** | No blocking delay > 10 ms in the super-loop; `_delay_ms` only in `*_Init()`. |
| **NFR-03** | No division, modulo or `float` inside any ISR. |
| **NFR-04** | Capture and overflow ISRs together ≤ 30 µs, so no edge is missed at 500 Hz. |
| **NFR-05** | Two SPI slaves are never selected simultaneously; every transaction is bracketed by `SPI_Acquire`/`SPI_Release`. |
| **NFR-06** | An EEPROM multi-step write is never interrupted by a lamp update. |
| **NFR-07** | No floating-point arithmetic anywhere. |
| **NFR-08** | All calibration constants (`pulsesPerRev`, `wheelCircMm`, …) come from `DashCfg_t`, never hard-coded in the speed maths. |
| **NFR-09** | Layer rule respected; only `MCAL/*.c` touches registers. |
| **NFR-10** | 32-bit shared values are read atomically. |
| **NFR-11** | Tick jitter ≤ ±1 ms; CPU load ≤ 60 % at 200 km/h (worst-case capture rate), measured on `PC6`. |
| **NFR-12** | `.data + .bss` ≤ 1 KB. |
| **NFR-13** | The odometer never decreases, never wraps, and is never lost by more than 100 m. |
| **NFR-14** | A torn EEPROM write leaves a previously valid slot intact. |
| **NFR-15** | The cluster reaches a defined state within 200 ms of any reset. |

---

## 15. Operating Modes

| State | Cluster lamps | LCD | Engine | Speed measured |
|-------|---------------|-----|--------|:--------------:|
| `CS_OFF` | Dark | Odometer 10 s then blank | Off | No |
| `CS_ACC` | Dark | Odometer + trip | Off | Yes |
| `CS_IGNITION` | Warnings only | Page | Off | Yes |
| `CS_BULBCHECK` | **All on** | `BULB CHECK` | Off | Yes |
| `CS_CRANKING` | Oil + battery | `CRANKING…` | Starting | Yes |
| `CS_RUNNING` | Active warnings | Selected page | Running | Yes |
| `CS_STALLED` | Oil + battery | `ENGINE STOPPED` | Stopped | Yes |
| `CS_LIMP_HOME` | Cause + check engine | `!! STOP ENGINE !!` | Running | Yes |

---

## 16. System Flow

```
   ┌──────────────┐
   │  Power ON    │
   └──────┬───────┘
          ▼
   ┌───────────────────────────────────────┐
   │ MCAL init: DIO, ADC, T0, T1(ICU), T2, │
   │ EXTI, USART, SPI, I2C                 │
   │ 595 cleared → all lamps off           │
   └──────┬────────────────────────────────┘
          ▼
   ┌───────────────────────────────────────┐
   │ Scan 8 EEPROM slots, pick the highest │
   │ valid sequence number → odometer      │
   └──────┬────────────────────────────────┘
          ▼
   ┌───────────────────────────────────────┐  none valid  ┌───────────┐
   │ Validate magic + checksum             ├─────────────▶│ Defaults, │
   └──────┬────────────────────────────────┘              │ odo = 0   │
          │ valid                                         └─────┬─────┘
          ▼◀─────────────────────────────────────────────────── ┘
   ┌───────────────────────────────────────┐
   │ CS_OFF: show odometer 10 s, sei()     │
   └──────┬────────────────────────────────┘
          ▼
╔═══════════════════════════════════════════════════════════╗
║             SUPER-LOOP (dispatch on 10 ms tick)           ║
║                                                           ║
║   10 ms → buttons, body switches, cluster FSM             ║
║   50 ms → warning evaluation, lamp byte → 74HC595         ║
║  100 ms → speed from capture delta, odometer integration  ║
║  250 ms → RPM window close, LCD repaint                   ║
║  500 ms → ADC gauges + filtering                          ║
║    1 s  → trip timers, chime scheduling                   ║
║    5 s  → diagnostics frame                               ║
║  event  → EEPROM odo save (100 m / 60 s / key-off)        ║
╚═══════════════════════════════════════════════════════════╝
```

---

## 17. State Machine

### 17.1 Diagram

```
                 ┌──────────┐
     key press   │  CS_OFF  │◀──────────── key held 2 s ────────┐
    ┌───────────▶│          │                                   │
    │            └────┬─────┘                                   │
    │                 │ key press                               │
    │                 ▼                                         │
    │            ┌──────────┐  key press   ┌───────────────┐    │
    │            │  CS_ACC  ├─────────────▶│ CS_IGNITION   │    │
    │            └──────────┘              └───────┬───────┘    │
    │                                              │ entry      │
    │                                              ▼            │
    │                                     ┌────────────────┐    │
    │                                     │ CS_BULBCHECK   │    │
    │                                     │     3 s        │    │
    │                                     └───────┬────────┘    │
    │                                             ▼             │
    │                                     ┌────────────────┐    │
    │                                     │  CS_IGNITION   │────┤
    │                                     └───────┬────────┘    │
    │                            START button     │             │
    │                                             ▼             │
    │                                     ┌────────────────┐    │
    │              5 s no start ◀─────────┤  CS_CRANKING   │    │
    │                                     └───────┬────────┘    │
    │                        RPM > 500 for 500 ms │             │
    │                                             ▼             │
    │                                     ┌────────────────┐    │
    │                                     │  CS_RUNNING    │────┘
    │                                     └───┬────────┬───┘
    │                   RPM < 300 for 1 s     │        │ critical warning
    │                                         ▼        ▼
    │                              ┌────────────┐  ┌───────────────┐
    └──────────────────────────────┤ CS_STALLED │  │ CS_LIMP_HOME  │
                     key held 2 s  └─────┬──────┘  └───────┬───────┘
                                         │ START           │ key off only
                                         └────────▶ CS_CRANKING
```

### 17.2 Transition table

| # | From | Event / guard | To | Actions |
|---|------|---------------|----|---------|
| T1 | `CS_OFF` | Key press | `CS_ACC` | Wake LCD, start speed measurement |
| T2 | `CS_ACC` | Key press | `CS_IGNITION` | `ignitionCycles++`, enter bulb check |
| T3 | `CS_IGNITION` | Entry | `CS_BULBCHECK` | All 8 lamps on, LCD `BULB CHECK` |
| T4 | `CS_BULBCHECK` | 3 s elapsed | `CS_IGNITION` | Lamps reflect real warnings |
| T5 | `CS_IGNITION` | Start button | `CS_CRANKING` | Start 5 s crank timer, oil+batt lamps |
| T6 | `CS_CRANKING` | RPM > 500 for 500 ms | `CS_RUNNING` | `engineRun = 1`, log `!EVT,ENGINE,START` |
| T7 | `CS_CRANKING` | 5 s elapsed | `CS_IGNITION` | Log `!EVT,CRANK,FAIL` |
| T8 | `CS_RUNNING` | RPM < 300 for 1 s | `CS_STALLED` | Log `!EVT,ENGINE,STALL` |
| T9 | `CS_STALLED` | Start button | `CS_CRANKING` | |
| T10 | `CS_RUNNING` | Critical warning (prio 1 – 3) | `CS_LIMP_HOME` | Latch, chime 5 s, log with snapshot |
| T11 | `CS_LIMP_HOME` | Key off only | `CS_OFF` | Save odometer, clear latches |
| T12 | any except `CS_OFF` | Key held 2 s | `CS_OFF` | **Force odometer save**, cluster dark |
| T13 | `CS_RUNNING` | Speed > limit | `CS_RUNNING` | Raise `WARN_OVERSPEED`, chime |
| T14 | any | No capture edge 1 s | same | `speedKmh = 0` |

---

## 18. UART Protocol

**Link:** 9600 8N1. Device sends `\r\n`; accepts `\r`, `\n`, `\r\n`.

### 18.1 Diagnostics frame (every 5 s)

```
$VD,S=62,R=2450,F=78,C=92,B=14200,O=34,ODO=123456,TRP=42700,MX=118,W=0021,ST=RUN,UP=2650*4B
```

| Field | Meaning | Units |
|-------|---------|-------|
| `S` | Road speed | km/h |
| `R` | Engine RPM | rpm |
| `F` | Fuel | % |
| `C` | Coolant | °C |
| `B` | Battery | mV |
| `O` | Oil pressure ×10 | 0.1 bar |
| `ODO` | Lifetime odometer | m |
| `TRP` | Trip distance | m |
| `MX` | Session max speed | km/h |
| `W` | `warnMask` | 4 hex digits |
| `ST` | `OFF`\|`ACC`\|`IGN`\|`BULB`\|`CRNK`\|`RUN`\|`STALL`\|`LIMP` | |
| `UP` | Seconds since key on | s |
| `*4B` | XOR checksum between `$` and `*` | |

### 18.2 Command set

| Command | Response | Effect |
|---------|----------|--------|
| `STATUS` | diagnostics frame | Immediate report |
| `SPEED?` | `SPEED=62` | |
| `RPM?` | `RPM=2450` | |
| `ODO?` | `ODO=123456` | Metres |
| `TRIP?` | `TRIP=42700,58,2640` | metres, avg km/h, seconds |
| `TRIPRESET` | `OK` | Zero the trip meter |
| `MAX?` | `MAX=118,187` | Session, all-time |
| `WARN?` | `WARN=0021,FUEL,OVERSPEED` | Mask plus names |
| `CFG?` | `CFG=120,10,110,10,12000,15000,4,2000,2` | All calibration values |
| `SET LIMIT <n>` | `OK` / `ERR RANGE` | 30 – 200 km/h |
| `SET FUELWARN <n>` | `OK` / `ERR RANGE` | 5 – 30 % |
| `SET COOLWARN <n>` | `OK` / `ERR RANGE` | 90 – 125 °C |
| `SET PPR <n>` | `OK` / `ERR RANGE` | Wheel pulses/rev, 1 – 32 |
| `SET CIRC <n>` | `OK` / `ERR RANGE` | Wheel circumference, 500 – 4000 mm |
| `SET TPR <n>` | `OK` / `ERR RANGE` | Tach pulses/rev, 1 – 8 |
| `SET ODO <n>` | `ERR PROTECTED` | **Always refused** (see FR-19) |
| `PAGE <0-4>` | `OK` / `ERR ARG` | Select display page |
| `LAMPTEST` | `OK` | Run a 3 s bulb check on demand |
| `SAVE` | `OK` / `ERR EEPROM` | Force a persistence write |
| `SLOTS?` | `SLOT=3,SEQ=1204` | Which wear-levelling slot is live |
| `HELP` | command list | |

### 18.3 Asynchronous events

```
!EVT,KEY,ON
!EVT,ENGINE,START
!EVT,CRANK,FAIL
!EVT,ENGINE,STALL
!EVT,WARN,OIL,SET,O=6
!EVT,WARN,OIL,CLR
!EVT,LIMP,COOLANT,C=113
!EVT,OVERSPEED,132
!EVT,MAXSPEED,132
!EVT,TRIP,RESET
!EVT,ODO,SAVE,SLOT=4
!EVT,SENSOR,CHECK,ADC2
!EVT,KEY,OFF,ODO=123456
```

---

## 19. EEPROM Data Layout

**Device:** 25LC256, SPI Mode 0, `SS` = `PB4`, page size 64 bytes.

### 19.1 Fixed configuration block

| Address | Size | Field | Type | Default |
|---------|:----:|-------|------|:-------:|
| `0x0000` | 2 | `magic` | `uint16_t` | `0x4443` |
| `0x0002` | 1 | `version` | `uint8_t` | `0x01` |
| `0x0003` | 2 | `speedLimitKmh` | `uint16_t` | 120 |
| `0x0005` | 1 | `fuelWarnPct` | `uint8_t` | 10 |
| `0x0006` | 1 | `coolantWarnC` | `uint8_t` | 110 |
| `0x0007` | 1 | `oilWarnBarX10` | `uint8_t` | 10 |
| `0x0008` | 2 | `battLowmV` | `uint16_t` | 12000 |
| `0x000A` | 2 | `battHighmV` | `uint16_t` | 15000 |
| `0x000C` | 1 | `pulsesPerRev` | `uint8_t` | 4 |
| `0x000D` | 2 | `wheelCircMm` | `uint16_t` | 2000 |
| `0x000F` | 1 | `tachPulsesPerRev` | `uint8_t` | 2 |
| `0x0010` | 2 | `maxSpeedRecord` | `uint16_t` | 0 |
| `0x0012` | 2 | `ignitionCycles` | `uint16_t` | 0 |
| `0x0014` | 1 | `lastPage` | `uint8_t` | 0 |
| `0x0015` | 1 | `checksum` | `uint8_t` | computed |
| `0x0016` – `0x003F` | 42 | reserved (`0xFF`) | — | — |

### 19.2 Odometer wear-levelling ring — 8 slots

The odometer is written far more often than anything else, so it lives in a
rotating set of eight slots. **Slot *n* is at `0x0040 + n*16`.**

| Offset | Size | Field | Type |
|:------:|:----:|-------|------|
| 0 | 2 | `slotMagic` | `uint16_t` = `0x4F44` (`'O','D'`) |
| 2 | 4 | `seq` | `uint32_t` — monotonically increasing |
| 6 | 4 | `odoMetres` | `uint32_t` |
| 10 | 4 | `tripMetres` | `uint32_t` |
| 14 | 1 | `reserved` | `uint8_t` |
| 15 | 1 | `slotCsum` | `uint8_t` — negative sum of bytes 0 – 14 |

**Boot algorithm**

```
best = none
for n in 0..7:
    read slot n
    if slotMagic ok and slotCsum ok and (best is none or seq > best.seq):
        best = slot n
if best is none: odo = 0, trip = 0, log CFG DEFAULT
else:            odo = best.odoMetres, trip = best.tripMetres
nextSlot = (best.index + 1) % 8
```

**Write algorithm**

```
seq       = best.seq + 1
target    = nextSlot
write slot[target]                  ← the previous slot stays intact
verify read-back; on failure, advance to the next slot and retry once
nextSlot  = (target + 1) % 8
```

Because the *previous* slot is never touched during a write, a power cut mid
write loses at most the newest 100 m and never corrupts the odometer (NFR-14).

### 19.3 Write budget

At 100 m per write and 8 slots, the 25LC256's ~1 M cycles per cell allow roughly
**800 000 km** of driving before wear becomes a concern. Show this calculation
in your report — it is the justification for the whole scheme.

---

## 20. Task Scheduling

| ID | Task | Period | Offset | Budget | Work |
|----|------|:------:|:------:|:------:|------|
| T-1 | `Task_Inputs` | 10 ms | 0 | 200 µs | Buttons, body switches, debounce |
| T-2 | `Task_FSM` | 10 ms | 0 | 200 µs | Cluster `switch` |
| T-3 | `Task_Lamps` | 50 ms | 1 | 400 µs | Warning eval + 595 shift + latch |
| T-4 | `Task_Speed` | 100 ms | 2 | 600 µs | Capture delta → km/h, odometer carry |
| T-5 | `Task_Tacho` | 250 ms | 3 | 150 µs | Close RPM window |
| T-6 | `Task_LCD` | 250 ms | 5 | 4 ms | Page repaint |
| T-7 | `Task_Gauges` | 500 ms | 4 | 1 ms | 4 ADC channels + filters |
| T-8 | `Task_1Hz` | 1 s | 6 | 400 µs | Trip timers, chime schedule |
| T-9 | `Task_Report` | 5 s | 8 | 2 ms | Diagnostics frame |
| T-10 | `Task_Console` | 20 ms | 7 | 500 µs | Parse one line |
| T-11 | `Task_EEPROM` | 10 ms | 9 | step | Odometer save state machine |

**Bus conflict:** T-3 and T-11 both want SPI. T-11 owns the bus for the duration
of a multi-step write; T-3 skips one cycle when the bus is busy. Show that a
skipped lamp refresh is invisible (50 ms → 100 ms is still flicker-free).

---

## 21. Testing Requirements

| ID | Test | Method | Pass criterion |
|----|------|--------|----------------|
| TC-01 | Blank EEPROM boot | Erase, power on | Odometer 0, defaults, no crash |
| TC-02 | Config persistence | `SET LIMIT 100`, `SAVE`, power cycle | 100 restored |
| TC-03 | Corrupted config | Flip a byte | Defaults loaded |
| TC-04 | Speed @ 200 km/h | Inject 111.1 Hz | Reads 200 ±1 km/h |
| TC-05 | Speed @ 60 km/h | Inject 33.3 Hz | Reads 60 ±1 km/h |
| TC-06 | Speed @ 10 km/h | Inject 5.6 Hz | Reads 10 ±1 km/h |
| TC-07 | **Low-speed capture extension** | Inject 1.1 Hz (≈ 2 km/h) | Reads 2 ±1 km/h, not garbage |
| TC-08 | Noise canceller | Add short glitches to `ICP1` | No false captures |
| TC-09 | Zero speed | Stop the pulse source | Reads 0 within 1.1 s |
| TC-10 | Restart after stall | Resume pulses | Valid speed within 2 edges, no spike |
| TC-11 | Speed plausibility | Inject 400 Hz | Reading rejected, `WARN_CHECK` |
| TC-12 | RPM @ 2400 | Inject 80 Hz | Reads 2400 ±60 |
| TC-13 | RPM @ 6000 | Inject 200 Hz | Reads 6000 ±60 |
| TC-14 | RPM window atomicity | Run 10 min at 3000 RPM | No sample outside ±120 |
| TC-15 | Odometer accuracy | 2000 pulses at 500 mm | Exactly +1000 m |
| TC-16 | Odometer no drift | Run 10 min at varying speed | Matches pulse count × 500 mm exactly |
| TC-17 | Odo save cadence | Drive 500 m, count writes | 5 writes ±1 |
| TC-18 | Odo persistence | Power cycle mid-drive | Loss ≤ 100 m |
| TC-19 | **Torn write** | Cut power during an EEPROM write | Previous slot valid, odometer sane |
| TC-20 | Wear levelling | 20 saves, `SLOTS?` | Slot index rotates 0 → 7 → 0 |
| TC-21 | Trip reset | Hold trip button 2 s | Trip zeroed, odometer unchanged |
| TC-22 | Trip vs. page | Short press trip button | Page changes, trip unchanged |
| TC-23 | Average speed | Drive 1 km in 60 s | `AVG` ≈ 60 km/h |
| TC-24 | Bulb check | Turn key to ignition | All 8 lamps on exactly 3 s ±100 ms |
| TC-25 | 595 latch timing | Scope `PC2` vs `SCK` | Latch after the 8th clock, ≥ 1 µs |
| TC-26 | **SPI arbitration** | Force a lamp refresh during an EEPROM write | No corruption on either device |
| TC-27 | CS exclusivity | Scope `PB4` and `PC2` | Never both active |
| TC-28 | Lamp flicker | Watch 60 s | No visible flicker |
| TC-29 | Warning priority | Force oil + fuel together | Banner = oil; both lamps lit; mask shows both |
| TC-30 | Critical latch | Trigger coolant, then cool it | Warning stays until key cycle |
| TC-31 | Limp home | Oil < 1.0 bar for 3 s while running | `CS_LIMP_HOME`, chime, banner |
| TC-32 | Turn indicators | Left switch on | 450 ms on/off ±20 ms |
| TC-33 | Hazard | Both switches on | Both lamps blink in phase |
| TC-34 | Ignition sequence | Key, key, start | `OFF→ACC→IGN→BULB→IGN→CRNK→RUN` |
| TC-35 | Crank timeout | Start with no RPM | Returns to `CS_IGNITION` after 5 s |
| TC-36 | Stall detect | Drop RPM to 0 while running | `CS_STALLED` within 1.1 s |
| TC-37 | Key off save | Hold key 2 s | Odometer written before the cluster goes dark |
| TC-38 | Over-speed | Exceed 120 km/h | Chime, lamp, repeat every 5 s |
| TC-39 | Over-speed hysteresis | Drop to 118 km/h | Warning stays; clears below 115 |
| TC-40 | Max speed record | Exceed the all-time record | `!EVT,MAXSPEED`, persists |
| TC-41 | Gauge scaling | Each pot at 0 / 50 / 100 % | Matches §11.3 within 1 % |
| TC-42 | Fuel damping | Swing the fuel pot quickly | Display moves smoothly, no jump |
| TC-43 | Sensor plausibility | Pin ADC3 at 1023 for 6 s | `WARN_CHECK`, gauge shows `---` |
| TC-44 | `SET ODO` refused | Send `SET ODO 0` | `ERR PROTECTED`, odometer unchanged |
| TC-45 | Console robustness | `FOO`, 40 chars, `SET LIMIT 500` | `ERR CMD`, `ERR LONG`, `ERR RANGE` |
| TC-46 | Telemetry cadence | Capture 60 s | 12 frames ±1, checksums valid |
| TC-47 | Tick jitter | Scope tick pin | 10 ms ±1 ms |
| TC-48 | CPU load at speed | 200 km/h, `PC6` duty | ≤ 60 % |
| TC-49 | RAM budget | `avr-size -C` | ≤ 1024 B |
| TC-50 | Soak | 15 min varying speed, RPM, faults | No hang, odometer monotonic |

---

## 22. Bonus Features

Maximum **+20**; final score capped at 100.

| # | Feature | Marks | Requirement |
|---|---------|:-----:|-------------|
| B1 | Fuel range estimate | +10 | Litres/100 km from trip data → `RANGE: 340 km`, hidden until 5 km of data exists |
| B2 | Analog-style bargraph tacho | +10 | Custom LCD characters forming a 16-segment RPM bar, updated at 10 Hz |
| B3 | Speed calibration wizard | +10 | `CALIB 1000` → drive a known 1000 m, firmware solves `wheelCircMm` and saves it |
| B4 | True cooperative scheduler | +15 | Task table with period/offset, SPI bus arbitration, overrun counter over UART |
| B5 | Watchdog recovery | +10 | WDT 250 ms, `MCUCSR` reason logged, odometer preserved across the reset |
| B6 | Backlight dimming | +10 | Timer PWM on `PC7`, driven by high-beam state and a light sensor |
| B7 | Fault-code memory | +10 | Store the last 8 warning events with odometer stamps; `DTC?` dumps them |
| B8 | Guarded odometer write | +5 | `UNLOCK <code>` then `SET ODO` allowed once per boot, logged loudly |

---

## 23. Deliverables

| # | Item | Detail |
|---|------|--------|
| 1 | Source code | Layered per §9.1; `SPI_Acquire`/`Release` used everywhere |
| 2 | `Simulation/dashboard.sim1` | Runs unmodified, both pulse sources adjustable |
| 3 | `Docs/timing_derivation.md` | Speed and RPM maths, resolution table, ISR budget |
| 4 | `Docs/flowchart.png` | Matches §16 |
| 5 | `Docs/state_machine.png` | Matches §17 with the transition table |
| 6 | `Docs/test_report.md` | All 50 `TC` rows with evidence |
| 7 | Final report | 15 – 20 pages incl. wear-levelling justification and the write-budget calculation |
| 8 | Demo video | 5 – 10 min: full ignition sequence, speed sweep incl. very low speed, torn-write recovery, limp home |
| 9 | Live defence | Any member, any file |

---

## 24. Evaluation Rubric

| Item | Marks | Full-mark criteria |
|------|:-----:|--------------------|
| GPIO | 5 | Own DIO driver; body switches debounced |
| ADC | 10 | Four channels, two damping strategies, correct `uint32_t` scaling |
| Timer | 10 | Input capture with 32-bit extension **and** a stable 10 ms tick |
| Interrupts | 5 | Capture, overflow and INT0 ISRs short and race-free |
| USART | 10 | Frame, events and parser per §18; `SET ODO` protected |
| SPI | 10 | Two slaves arbitrated correctly; wear-levelled odometer survives a torn write |
| I2C | 10 | LCD via PCF8574, five pages, flicker-free |
| Application logic | 20 | Ignition FSM, warning priority, limp home, odometer accuracy |
| Architecture | 10 | Layer rule; calibration constants never hard-coded |
| Testing | 10 | 50 cases including TC-07, TC-19 and TC-26 |
| Documentation & demo | 10 | Timing derivation present; low-speed and torn-write demos shown live |
| **Total** | **100** | Bonus up to +20, capped at 100 |

---

*Prepared by Ahmed Ellamie | ahmed.ellamiee@gmail.com*
??? ??????? ?? ????? ???? ?????? ????? ????? ???? ?????? ???????.
