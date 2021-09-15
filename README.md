# samd51-micromod-bitbang
## SPI Flash Bit-Bang for the v1.2 SAMD51 MicroMod

The v1.2 SAMD51 MicroMod boards have a layout error that
swaps the MISO/MOSI pins going to the external flash.
Those pins can't be re-muxed internally on the chip,
so SPI is unavailable for use. This library replaces
SPI calls with a bit-bang transfer routine, and is based
on the Adafruit SPIFlash library.

#### Transfer speed is ~1.429MHz

### Available flash read/write functions
- begin(): Initialize the chip and save the jedec-id.
- read8(n): Reads 8-bit value from address n
- read16(n): "" 16-bit ""
- read32(n): "" 32-bit ""
- writeBuffer(addr, *buff, len): Writes *buff to addr + len.
- eraseChip(): Erases entire chip. Can take upwards of 40 seconds ( from W25Q128 datasheet).
- eraseBlock(n): Erases a 64k block at blocknumber n.

[TO DO]
- Full compatibility with SPIFlash library not complete (missing functions).
