# Remote I/O Zephyr

Zephyr-based remote I/O firmware for the STM32 Nucleo-F767ZI. The device exposes a simple text protocol over TCP so a client can read inputs, drive outputs, exchange UART data, control a WS2812 LED strip, and update persistent network or UART settings.

## What this firmware provides

- 16 digital inputs
- 16 digital outputs
- 2 UART channels
- 1 WS2812 LED strip output with 25 LEDs
- Persistent network and serial settings stored in flash
- OTA support through the Mender MCU client

## Build and run

With a Zephyr workspace already initialized:

```sh
west build -b nucleo_f767zi .
west flash
```

Factory-default network values from `src/settings.c`:

| Item | Value |
| --- | --- |
| IP address | `192.168.1.10` |
| Netmask | `255.255.255.0` |
| TCP port offset (`R102`) | `0` |
| Effective TCP listen port | `8500` |

Example connection:

```sh
nc 192.168.1.10 8500
```

## Protocol overview

Commands are ASCII text terminated by `\r\n`.

### Command format

```text
[R|W]<id>[.<variant>] [param1] [param2] ... [paramN]\r\n
```

- `R` reads data.
- `W` writes data.
- `.<variant>` is optional and used by commands such as UART and bulk output writes.
- `R` and `W` are case-insensitive.

### Reply format

Typical replies:

```text
R<id> <value...>\r\n
W<id> OK\r\n
ERR<code>\r\n
```

Asynchronous notifications:

```text
S5 <pin> <state>\r\n
R7.<uart> <payload>\r\n
```

- `S5` is emitted when a subscribed input changes.
- `R7.<uart>` is emitted when data is received on a UART channel.

## Supported API

### Services

| ID | Name | Access | Notes |
| --- | --- | --- | --- |
| `1` | Status | Read | Returns the current firmware status string such as `OK` or `UPDATING`. |
| `2` | System info | Read | Returns a multi-line summary of firmware version and available resources. |
| `3` | Digital input | Read | Read one input or all inputs. |
| `4` | Digital output | Read/Write | Read one output, read all outputs, write one output, or write a range with variant `.1`. |
| `5` | Input subscription | Read/Write | Read subscribed pins or subscribe one or more inputs. |
| `6` | Input unsubscription | Write | Unsubscribe one or more inputs. |
| `7.<uart>` | UART | Write + async readback | Send bytes to a UART channel; received UART bytes are forwarded asynchronously. |
| `8` | WS2812 LED | Read/Write | Read or set one LED color. |

### Settings currently implemented

| ID | Name | Access | Notes |
| --- | --- | --- | --- |
| `101` | IP address | Read/Write | Stored persistently in flash. |
| `102` | TCP port offset | Read/Write | Effective listen port is `8500 + offset`. |
| `103` | Netmask | Read/Write | Stored persistently in flash. |
| `104` | Gateway | Read/Write | Stored persistently in flash. |
| `105` | MAC address | Read/Write | Stored persistently in flash. |
| `106.<uart>` | UART baud rate | Read/Write | Supported rates: `9600`, `19200`, `38400`, `57600`, `115200`. |

> Note: IDs `9`, `10`, and settings `107`-`111` exist in headers but are not currently handled by `src/api.c`.

## Command reference

### 1. Status (`R1`)

```text
R1
```

Example reply:

```text
R1 OK
```

The returned string is one of the values listed in the [Status codes](#status-codes) section.

### 2. System info (`R2`)

```text
R2
```

This returns a multi-line text block, for example:

```text
Firmware: <version>
Commands:
  Read: R<service_id> <param1> ...
  Write: W<service_id> <param1> ...

System Info:
  <resource summary>
```

### 3. Digital inputs (`R3`)

Read one input:

```text
R3 <pin>
```

Read all inputs as a bitfield:

```text
R3 -1
```

Examples:

```text
R3 1
R3 -1
```

Possible replies:

```text
R3 1 0
R3 13
```

For `R3 -1`, bit 0 corresponds to input 1, bit 1 to input 2, and so on.

### 4. Digital outputs (`R4`, `W4`, `W4.1`)

Read one output:

```text
R4 <pin>
```

Read all outputs as a bitfield:

```text
R4 -1
```

Write one output:

```text
W4 <pin> <0|1>
```

Write multiple outputs starting at a pin:

```text
W4.1 <data> <start_pin> <length>
```

Examples:

```text
R4 6
R4 -1
W4 4 1
W4.1 41 2 8
```

- In `W4.1`, bit 0 of `data` maps to `start_pin`, bit 1 to the next pin, and so on.
- `W4.1 41 2 8` applies `0b00101001` to outputs 2 through 9.

### 5. Subscribe to input changes (`R5`, `W5`)

Read the current subscription list:

```text
R5
```

Subscribe one or more pins:

```text
W5 <pin> [pin...]
```

Examples:

```text
R5
W5 2
W5 2 7 9
```

Reply and event examples:

```text
R5 2 7 9
W5 OK
S5 7 1
```

### 6. Unsubscribe from input changes (`W6`)

```text
W6 <pin> [pin...]
```

Examples:

```text
W6 2
W6 2 7 9
```

Reply:

```text
W6 OK
```

### 7. UART bridge (`W7.<uart>`)

Send a payload to a UART channel:

```text
W7.<uart> <length> <payload>
```

Examples:

```text
W7.0 5 hello
W7.1 11 hello world
```

Replies:

```text
W7 OK
R7.0 hello
R7.1 hello world
```

Notes:

- UART variants are currently zero-based in this firmware:
  - `.0` -> USART2
  - `.1` -> UART5
- `<length>` is the number of payload bytes that follow.
- The payload is length-delimited, so it may contain spaces as long as the byte count is correct.

### 8. WS2812 LED control (`R8`, `W8`)

Read one LED:

```text
R8 <led_index>
```

Write one LED:

```text
W8 <led_index> <r> <g> <b>
```

Examples:

```text
R8 4
W8 19 255 255 0
```

Possible replies:

```text
R8 4 127 23 255
W8 OK
```

LED indices are zero-based. With the current board overlay, valid indices are `0` through `24`.

### 101. IP address (`R101`, `W101`)

```text
R101
W101 192 168 1 20
```

Reply examples:

```text
R101 192 168 1 10
W101 OK
```

### 102. TCP port offset (`R102`, `W102`)

```text
R102
W102 3
```

Reply examples:

```text
R102 0
W102 OK
```

The device listens on `8500 + offset`, so `W102 3` changes the effective TCP port to `8503`.

### 103. Netmask (`R103`, `W103`)

```text
R103
W103 255 255 255 0
```

### 104. Gateway (`R104`, `W104`)

```text
R104
W104 192 168 1 1
```

### 105. MAC address (`R105`, `W105`)

```text
R105
W105 0 5 79 1 2 3
```

Replies use six decimal octets:

```text
R105 0 5 79 1 2 3
```

### 106. UART baud rate (`R106.<uart>`, `W106.<uart>`)

```text
R106.0
W106.0 115200
R106.1
W106.1 9600
```

Reply examples:

```text
R106.0 19200
W106 OK
R106.1 9600
```

## Hardware mapping

### Digital inputs

| Input | MCU pin |
| --- | --- |
| 1 | `PE0` |
| 2 | `PE2` |
| 3 | `PE3` |
| 4 | `PE4` |
| 5 | `PE5` |
| 6 | `PE6` |
| 7 | `PE7` |
| 8 | `PE8` |
| 9 | `PE10` |
| 10 | `PF0` |
| 11 | `PF1` |
| 12 | `PF2` |
| 13 | `PF4` |
| 14 | `PF7` |
| 15 | `PF8` |
| 16 | `PF9` |

### Digital outputs

| Output | MCU pin |
| --- | --- |
| 1 | `PC7` |
| 2 | `PC8` |
| 3 | `PC9` |
| 4 | `PC10` |
| 5 | `PC11` |
| 6 | `PC12` |
| 7 | `PD0` |
| 8 | `PD1` |
| 9 | `PD2` |
| 10 | `PD3` |
| 11 | `PD4` |
| 12 | `PD6` |
| 13 | `PD7` |
| 14 | `PD11` |
| 15 | `PD12` |
| 16 | `PD13` |

### UART channels

| API variant | Peripheral | TX | RX | Default baud |
| --- | --- | --- | --- | --- |
| `0` | `USART2` | `PD5` | `PA3` | `19200` |
| `1` | `UART5` | `PB6` | `PB12` | `9600` |

### WS2812 output

| Channel | MCU pin | LED count |
| --- | --- | --- |
| 1 | `PA6` | 25 |

## Status codes

The firmware status is represented by the `system_status_t` enum and converted to a string by `system_info_status_code_to_string`. The possible values are:

| Status string | Meaning |
| --- | --- |
| `OK` | System is operating normally. |
| `ERROR` | A general error has occurred. |
| `CHECKING_FOR_UPDATE` | The Mender client is contacting the server to check whether a firmware update is available. This is also the initial state at boot. |
| `UPDATE_AVAILABLE` | A firmware update has been found and is ready to be applied. |
| `UPDATING` | A firmware update is in progress. |
| `MENDER_DOWNLOADING` | The Mender client is downloading the update artifact. |
| `MENDER_INSTALLING` | The Mender client is writing the artifact to flash. |
| `MENDER_REBOOTING` | The device is about to reboot to apply the update. |

## Error codes

The firmware reports protocol and execution errors as:

```text
ERR<code>
```

| Code | Meaning |
| --- | --- |
| `200` | Failed to allocate memory for a parsed token |
| `201` | Invalid command type |
| `202` | Invalid command ID |
| `203` | Invalid command variant |
| `204` | Invalid command parameter |
| `205` | Too many digits in a numeric parameter |
| `206` | Failed to update IP address |
| `207` | Failed to update TCP port |
| `208` | Failed to update netmask |
| `209` | Failed to update gateway |
| `210` | Failed to update MAC address |
| `211` | Failed to update UART baud rate |
| `212` | Failed to update UART data bits |
| `213` | Failed to update UART parity |
| `214` | Failed to update UART stop bits |
| `215` | Failed to update UART flow control |
| `216` | Failed to update number of LEDs |
| `217` | Failed to set LED color |
| `218` | Failed to update LED state |
| `219` | Failed to write a digital output |
| `220` | Failed to read LED color |
| `221` | System not ready |
