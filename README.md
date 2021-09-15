# samd51-micromod-bitbang
## SPI Flash Bit-Bang for the v1.2 SAMD51 MicroMod

The v1.2 SAMD51 MicroMod boards have a layout error that
swaps the MISO/MOSI pins going to the external flash.
Those pins can't be re-muxed internally on the chip,
so SPI is unavailable for use. This library replaces
SPI calls with a bit-bang transfer routine, and is based
on the Adafruit SPIFlash library.

#### Transfer speed is ~1.429MHz

[TO DO]
- Full compatibility with SPIFlash library not complete (missing functions).
