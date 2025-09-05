# ESP32 Antenna Switch 

for the Remote Club Station of [DARC Bremen (i04)](https://darc.de/i04)\
\
**This is a draft!** \
There is no guarantee of functionality. Use at your own risk. 

**ToDos**
 - BOM
 - 3D design for FDM printers and shielding
 - Arduino Code
 - first builds

## Description

The antenna switch can be controlled via the ESP32 either by USB, WiFi, or Bluetooth, depending on what is implemented in the code. Two buttons can be connected for local control. The switched inputs and outputs are also indicated by LEDs. 

The 3 inputs and 5 outputs are switched using two bistable relays per connection (Finder 40.31.6.006.0000). These are controlled by drivers that can be put into sleep mode between switching operations (DRV8833PWP). This reduces power consumption and minimizes interference from busy switching regulators. In addition, the USB port of the ESP32 is sufficient for the power supply. 

The relay drivers are controlled via an I2C 16-bit port expander (MCP23017). 
In addition, a PWR/SWR meter is available via an I2C AD converter (ADS1115IDGS). This is used to prevent switching during transmission. It can also be used for transmitter-independent measurement to display transmission power and SWR in the remote software. 

Since both the relay switching and the reading of the SWR bridge are communicated via I2C, other controllers can also be used if necessary. It is also possible to connect several boards together. For this purpose, the I2C addresses can be easily adjusted via the solder pads. 

## Usage

**Kicad 9:**
You maybe need the KiCad symbol and footprint for the ESP32 Mini(Wemos D1 Mini) [https://github.com/r0oland/ESP32_mini_KiCad_Library](https://github.com/r0oland/ESP32_mini_KiCad_Library) - Thanks to [Johannes Rebling](https://github.com/r0oland)

## Contributing

Pull requests are welcome. For major changes, please open an issue first
to discuss what you would like to change.

## License

[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) 
