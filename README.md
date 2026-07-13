# SI4732 Radio

SI4732 Radio is a custom firmware project designed to power my home-built radio receiver.

Based on [G8PTN's ATS_MINI](https://github.com/G8PTN/ATS_MINI).

## Overview

It harnesses the capabilities of the SI4732 module to deliver high-quality FM radio performance, while the innovative design optimizes simple hardware into a complete and versatile receiver platform.

[<img src="https://img.youtube.com/vi/iMb66FNOmYM/hqdefault.jpg" width="560" />](https://www.youtube.com/watch?v=iMb66FNOmYM)
[<img src="https://img.youtube.com/vi/ZbDGKxzLqTs/hqdefault.jpg" width="560" />](https://www.youtube.com/watch?v=ZbDGKxzLqTs)

## Build

<img src="./img/img_1.jpg" alt="Radio 1" width="560" />
<img src="./img/img_2.jpg" alt="Radio 2" width="560" />
<img src="./img/img_3.jpg" alt="Radio 3" width="560" />
<img src="./img/img_4.jpg" alt="Radio 4" width="560" />
<img src="./img/img_5.jpg" alt="Radio 5" width="560" />
<img src="./img/img_6.jpg" alt="Radio 6" width="560" />
<img src="./img/img_7.jpg" alt="Radio 7" width="560" />
<img src="./img/img_8.jpg" alt="Radio 8" width="560" />

### Schematics
<img src="./img/SCH_Schematic_1-P1.png" alt="Schematics" width="560" />

### [3D Case](https://www.tinkercad.com/things/8lZGNjQrsIt-si4732-radio?sharecode=HUZ-hwZfG91KAuRBbbe97_qAXLUEwu6S0yEopXSdGyc)

### [LS027B7DH01 Breakout board](https://github.com/ddB0515/LS027B7DH01-Breakout-board)

## Flashing precompiled firmware (no build required)

Precompiled binaries are available in the [firmware/](./firmware/) folder, so you don't need to build the project to try it out.

### Requirements

- Python 3 with [esptool](https://github.com/espressif/esptool) installed:

  ```sh
  pip install esptool
  ```
- An ESP32-S2 Mini (Lolin S2 Mini) connected via USB, in bootloader mode if required (hold BOOT while plugging in, if the board isn't detected).

### Flash

Replace `COMx` (Windows) / `/dev/ttyUSBx` or `/dev/cu.usbmodemxxxx` (Linux/macOS) with your board's serial port:

```sh
esptool.py --chip esp32s2 --port COMx --baud 921600 write_flash -z \
  0x1000 firmware/bootloader.bin \
  0x8000 firmware/partitions.bin \
  0xe000 firmware/boot_app0.bin \
  0x10000 firmware/firmware.bin
```

After flashing completes, reset the board (or unplug/replug) to start the new firmware.

## Hardware Components

- **ESP32-S2 Mini**
- **SI4732 Module**
- **360-Degree Rotary Encoder Module**
- **Sharp LS027B7DH01 LCD:** A 2.7" display with 400x240 resolution.

## Project Inspiration and Credits

This project draws inspiration from several key contributions in the community:

- [G8PTN's ATS_MINI](https://github.com/G8PTN/ATS_MINI)
- [Ralph Xavier](https://github.com/ralphxavier/SI4735)
- [PU2CLR, Ricardo](https://github.com/pu2clr/SI4735)
- [Goshante](https://github.com/goshante/ats20_ats_ex)

Special thanks to G8PTN for the original ATS_MINI firmware, which served as the cornerstone for this adaptation.

## License

This project is released under the [MIT License](https://github.com/joaquimorg). The full license text is provided below:

    MIT License

    Copyright 2025 joaquim.org

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.