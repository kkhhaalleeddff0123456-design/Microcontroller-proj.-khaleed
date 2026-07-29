# Driver Function Map

---

## MCL (Microcontroller Layer)

### GPIO
| Function | What it does | Parameters |
|---|---|---|
| `GPIO_SetPinDirection` | Set a single pin as INPUT or OUTPUT | `port`, `pin`, `direction` |
| `GPIO_SetPortDirection` | Set all pins on a port as INPUT or OUTPUT | `port`, `direction` |
| `GPIO_GetPinStatus` | Read the current value of a pin | `port`, `pin` |
| `GPIO_GetPortStatus` | Read the current value of a whole port | `port` |
| `GPIO_PinToggle` | Flip a pin's output | `port`, `pin` |
| `GPIO_SetPinValue` | Write HIGH or LOW to a pin | `port`, `pin`, `value` |
| `GPIO_SetPortValue` | Write a byte to a whole port | `port`, `value` |

---

### UART
| Function | What it does | Parameters |
|---|---|---|
| `UART_Init` | Set up baud rate, data bits, parity, stop bits | `*config` |
| `UART_DeInit` | Disable TX and RX | — |
| `UART_SendByte` | Blocking send of one byte | `byte` |
| `UART_ReceiveByte` | Blocking receive of one byte | `*byte` |
| `UART_ReceiveByteNonBlocking` | Return a byte only if one is ready, else return E_NOK | `*byte` |
| `UART_SendString` | Blocking send of a null-terminated string | `*string` |
| `UART_ReceiveString` | Receive chars into a buffer until a terminator byte | `*buffer`, `maxLength`, `terminator` |
| `UART_SetRxCallBack` | Register a function to call on every RX interrupt | `*callback` |

---

### ADC
| Function | What it does | Parameters |
|---|---|---|
| `ADC_Init` | Set reference voltage and prescaler, enable ADC | `*config` |
| `ADC_DeInit` | Disable the ADC | — |
| `ADC_StartConversion` | Select a channel and start a conversion (non-blocking) | `channel` |
| `ADC_IsConversionComplete` | Check if the last conversion is done | — |
| `ADC_ReadResult` | Read the 10-bit result of the last conversion | `*result` |
| `ADC_ReadChannelBlocking` | Start a conversion and block until result is ready | `channel`, `*result` |

---

### I2C
| Function | What it does | Parameters |
|---|---|---|
| `I2C_InitMaster` | Set up TWI as master with a given SCL frequency | `*config` |
| `I2C_InitSlave` | Set up TWI as slave with an address | `*config` |
| `I2C_DeInit` | Disable the TWI peripheral | — |
| `I2C_Start` | Send a START condition | — |
| `I2C_Stop` | Send a STOP condition | — |
| `I2C_WriteByte` | Transmit one byte (address or data) | `byte` |
| `I2C_ReadByteWithAck` | Read one byte and send ACK (more bytes coming) | `*byte` |
| `I2C_ReadByteWithNack` | Read one byte and send NACK (last byte) | `*byte` |
| `I2C_GetStatus` | Return the current TWI status code | — |
| `I2C_MasterWrite` | High-level: START → SLA+W → send buffer → STOP | `address`, `*data`, `length` |
| `I2C_MasterRead` | High-level: START → SLA+R → read buffer → STOP | `address`, `*buffer`, `length` |

---

### SPI
| Function | What it does | Parameters |
|---|---|---|
| `SPI_Init` | Configure role, polarity, phase, bit order, clock rate | `*config` |
| `SPI_DeInit` | Disable the SPI peripheral | — |
| `SPI_Transceive` | Send a byte and receive a byte at the same time (blocking) | `txByte`, `*rxByte` |
| `SPI_SendByte` | Send one byte, discard what comes back | `byte` |
| `SPI_SendString` | Send a null-terminated string byte by byte | `*string` |
| `SPI_SetCallBack` | Register a function to call on transfer-complete interrupt | `*callback` |

---

### Timer
| Function | What it does | Parameters |
|---|---|---|
| `Timer_Init` | Configure channel, mode, prescaler, initial and compare values | `*config` |
| `Timer_DeInit` | Stop a channel and reset its registers | `channel` |
| `Timer_Start` | Connect the clock so the timer starts counting | `channel`, `prescaler` |
| `Timer_Stop` | Disconnect the clock, counter value is kept | `channel` |
| `Timer_SetCounterValue` | Write a value into TCNTx | `channel`, `value` |
| `Timer_GetCounterValue` | Read the current TCNTx value | `channel`, `*value` |
| `Timer_SetCompareValue` | Write a value into OCRx (used in CTC/PWM) | `channel`, `value` |
| `Timer_EnableInterrupt` | Enable overflow or compare-match interrupt | `channel`, `intType` |
| `Timer_DisableInterrupt` | Disable overflow or compare-match interrupt | `channel`, `intType` |
| `Timer_SetCallBack` | Register a function to call from the timer ISR | `channel`, `intType`, `*callback` |
| `Timer_EnableGlobalInterrupt` | Set the global interrupt enable bit (sei) | — |
| `Timer_DisableGlobalInterrupt` | Clear the global interrupt enable bit (cli) | — |

---

### Interrupt (EXTI)
| Function | What it does | Parameters |
|---|---|---|
| `EXTI_Init` | Set sense control for a line and enable it | `*config` |
| `EXTI_Enable` | Unmask an interrupt line in GICR | `line` |
| `EXTI_Disable` | Mask an interrupt line in GICR | `line` |
| `EXTI_SetSenseControl` | Change the trigger condition without touching enable state | `line`, `sense` |
| `EXTI_SetCallBack` | Register a function to call when the line fires | `line`, `*callback` |
| `EXTI_EnableGlobalInterrupt` | Set the global interrupt enable bit (sei) | — |
| `EXTI_DisableGlobalInterrupt` | Clear the global interrupt enable bit (cli) | — |

---

## HAL (Hardware Abstraction Layer)

### DC Motor
| Function | What it does | Parameters |
|---|---|---|
| `DC_Motor_Init` | Set up bridge pins and PWM channel, motor starts stopped | `*handle` |
| `DC_Motor_SetSpeed` | Set speed as 0–100% duty cycle | `*handle`, `speedPercent` |
| `DC_Motor_Forward` | Drive motor forward at current speed | `*handle` |
| `DC_Motor_Backward` | Drive motor backward at current speed | `*handle` |
| `DC_Motor_SetDirection` | Set direction without changing speed | `*handle`, `direction` |
| `DC_Motor_Stop` | Coast to stop (both inputs low) | `*handle` |
| `DC_Motor_Brake` | Hard stop (both inputs high, shorts windings) | `*handle` |
| `DC_Motor_GetState` | Read the last commanded state (STOP/FORWARD/BACKWARD/BRAKE) | `*handle`, `*state` |
| `DC_Motor_GetSpeed` | Read the last commanded speed percentage | `*handle`, `*speed` |
| `DC_Motor_DeInit` | Stop motor and release its PWM channel | `*handle` |

---

### Keypad
| Function | What it does | Parameters |
|---|---|---|
| `Keypad_Init` | Set row pins as outputs and column pins as inputs | `*config` |
| `Keypad_GetKey` | Non-blocking scan, returns pressed key or `KEYPAD_NO_KEY` | `*config`, `*key` |
| `Keypad_WaitForKey` | Block until a key is pressed, then return it | `*config`, `*key` |

---

### LCD (Generic HD44780 GPIO)
| Function | What it does | Parameters |
|---|---|---|
| `LCD_Init` | Run power-on sequence, set bus width, clear display | `*config` |
| `LCD_SendCommand` | Send a raw instruction byte (RS=0) | `*config`, `command` |
| `LCD_WriteChar` | Write one character at the cursor | `*config`, `character` |
| `LCD_WriteString` | Write a null-terminated string at the cursor | `*config`, `*string` |
| `LCD_WriteNumber` | Write a signed integer as decimal text | `*config`, `number` |
| `LCD_SetCursor` | Move cursor to (row, column) | `*config`, `row`, `col` |
| `LCD_Clear` | Clear display and return cursor home | `*config` |
| `LCD_CreateCustomChar` | Store a 5×8 glyph in CGRAM slot 0–7 | `*config`, `slot`, `*pattern` |

---

### LCD HD44780 (Handle-based GPIO)
| Function | What it does | Parameters |
|---|---|---|
| `LCD_Hd44780_Init` | Power-on reset sequence, configure bus, clear display | `*handle` |
| `LCD_Hd44780_SendCommand` | Send a raw instruction byte | `*handle`, `command` |
| `LCD_Hd44780_WriteChar` | Write one character at the cursor | `*handle`, `character` |
| `LCD_Hd44780_WriteString` | Write a null-terminated string at the cursor | `*handle`, `*string` |
| `LCD_Hd44780_WriteStringAt` | Move to (row, col) then write a string | `*handle`, `row`, `col`, `*string` |
| `LCD_Hd44780_WriteNumber` | Write a signed integer as decimal text | `*handle`, `number` |
| `LCD_Hd44780_SetCursor` | Move cursor to (row, column) | `*handle`, `row`, `col` |
| `LCD_Hd44780_Clear` | Clear display and return cursor home | `*handle` |
| `LCD_Hd44780_Home` | Return cursor to (0,0) without erasing | `*handle` |
| `LCD_Hd44780_DisplayOnOff` | Turn display on or off | `*handle`, `on` |
| `LCD_Hd44780_CursorOnOff` | Show or hide the underline cursor | `*handle`, `on` |
| `LCD_Hd44780_BlinkOnOff` | Enable or disable blinking block cursor | `*handle`, `on` |
| `LCD_Hd44780_ShiftDisplay` | Scroll the display window left or right | `*handle`, `toRight` |
| `LCD_Hd44780_CreateCustomChar` | Store a 5×8 glyph in CGRAM slot 0–7 | `*handle`, `slot`, `*pattern` |

---

### LCD AiP31068 (I2C)
| Function | What it does | Parameters |
|---|---|---|
| `LCD_Aip31068_Init` | Power-on init over I2C, clear display | `*handle` |
| `LCD_Aip31068_SendCommand` | Send a raw instruction byte over I2C | `*handle`, `command` |
| `LCD_Aip31068_WriteChar` | Write one character at the cursor | `*handle`, `character` |
| `LCD_Aip31068_WriteString` | Write a null-terminated string in one I2C transaction | `*handle`, `*string` |
| `LCD_Aip31068_WriteStringAt` | Move to (row, col) then write a string | `*handle`, `row`, `col`, `*string` |
| `LCD_Aip31068_WriteNumber` | Write a signed integer as decimal text | `*handle`, `number` |
| `LCD_Aip31068_SetCursor` | Move cursor to (row, column) | `*handle`, `row`, `col` |
| `LCD_Aip31068_Clear` | Clear display and home cursor | `*handle` |
| `LCD_Aip31068_Home` | Return cursor to (0,0) without erasing | `*handle` |
| `LCD_Aip31068_DisplayOnOff` | Turn display on or off | `*handle`, `on` |
| `LCD_Aip31068_CursorOnOff` | Show or hide the underline cursor | `*handle`, `on` |
| `LCD_Aip31068_BlinkOnOff` | Enable or disable blinking block cursor | `*handle`, `on` |
| `LCD_Aip31068_ShiftDisplay` | Scroll the display window left or right | `*handle`, `toRight` |
| `LCD_Aip31068_CreateCustomChar` | Store a 5×8 glyph in CGRAM slot 0–7 | `*handle`, `slot`, `*pattern` |

---

### Servo Motor
| Function | What it does | Parameters |
|---|---|---|
| `Servo_Motor_Init` | Configure Timer1 (hardware) or GPIO pin (software), park at centre | `*handle` |
| `Servo_Motor_SetAngle` | Command an angle 0–maxAngle degrees | `*handle`, `angle` |
| `Servo_Motor_SetPulseUs` | Command a raw pulse width in microseconds | `*handle`, `pulseUs` |
| `Servo_Motor_GetAngle` | Read the last commanded angle | `*handle`, `*angle` |
| `Servo_Motor_Stop` | Stop sending pulses, servo goes limp | `*handle` |
| `Servo_Motor_Start` | Resume pulsing at the last commanded angle | `*handle` |
| `Servo_Motor_SoftwareRefresh` | Emit one pulse per software servo — call every 20 ms | — |
| `Servo_Motor_DeInit` | Stop pulses and free the servo's slot/channel | `*handle` |

---

### Seven Segment
| Function | What it does | Parameters |
|---|---|---|
| `SevenSeg_Init` | Set segment/data pins as outputs and blank the display | `*config` |
| `SevenSeg_DisplayDigit` | Show a decimal digit 0–9 | `*config`, `digit` |
| `SevenSeg_Clear` | Turn all segments off | `*config` |
| `SevenSeg_EnableDigit` | Turn a digit's enable/common line ON (multiplexing) | `port`, `pin`, `activeLevel` |
| `SevenSeg_DisableDigit` | Turn a digit's enable/common line OFF (multiplexing) | `port`, `pin`, `activeLevel` |

---

### Stepper Motor (L298P)
| Function | What it does | Parameters |
|---|---|---|
| `Stepper_L298P_Init` | Set coil pins as outputs, de-energize, zero position counter | `*handle` |
| `Stepper_L298P_SetStepMode` | Change excitation mode (WAVE / FULL / HALF) at runtime | `*handle`, `mode` |
| `Stepper_L298P_SetStepDelay` | Set milliseconds between steps (raw speed control) | `*handle`, `delayMs` |
| `Stepper_L298P_SetSpeedRpm` | Set speed in RPM, converts to step delay automatically | `*handle`, `rpm` |
| `Stepper_L298P_Step` | Move N steps in a direction, blocking | `*handle`, `steps`, `dir` |
| `Stepper_L298P_StepOnce` | Advance exactly one step and return immediately (non-blocking) | `*handle`, `dir` |
| `Stepper_L298P_RotateAngle` | Rotate by degrees, blocking | `*handle`, `degrees`, `dir` |
| `Stepper_L298P_Hold` | Re-energize current phase for holding torque without moving | `*handle` |
| `Stepper_L298P_Release` | Cut current to all coils, shaft turns freely | `*handle` |
| `Stepper_L298P_GetPosition` | Read net step count since Init or last ResetPosition | `*handle`, `*position` |
| `Stepper_L298P_ResetPosition` | Zero the position counter | `*handle` |
| `Stepper_L298P_GetStepsPerRev` | Get steps per revolution in the active mode | `*handle`, `*stepsPerRev` |
