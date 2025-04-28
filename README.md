# atomsync

A digital clock and alarm application designed for the Kinetis K60 microcontroller (using the FITkit 3 platform).

## Features

*   Displays current time.
*   Alarm functionality with customizable time, melodies, and light effects.
*   Configurable alarm repetition.
*   UART-based terminal interface for interaction.

## Build

Run the following command:

```bash
make
```

## Usage

1.  Connect the FITkit 3 board to your computer via the USB Type B port.
2.  Open a terminal emulator (e.g., PuTTY, minicom, screen) and connect to the serial port associated with the FITkit 3. Configure the serial connection settings (baud rate, data bits, parity, stop bits) as required by the application (refer to `UARTInit` function in `main.c` if unsure - common settings are 9600 or 115200 baud, 8N1).
3.  Once connected, the application should display a menu or prompt.
4.  Follow the on-screen prompts to:
    *   Set the current time (`setClock`).
    *   Set the alarm time (`setAlarm`).
    *   Enable/disable the alarm (`toggleAlarm`).
    *   Choose alarm melody (`chooseMelody`).
    *   Choose alarm light effect (`chooseLightEffect`).
    *   Configure alarm repeat settings (`setAlarmRepeat`).