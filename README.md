# samd51-micromod-bitbang
## SPI Flash Bit-Bang for the v1.2 SAMD51 MicroMod

The v1.2 SAMD51 MicroMod boards have a layout error that
swaps the MISO/MOSI pins going to the external flash.
Those pins can't be re-muxed internally on the chip,
so SPI is unavailable for use. This library replaces
SPI calls with a bit-bang transfer routine, and is based
on the Adafruit SPIFlash library.

#### Transfer speed is now ~4.7MHz, up from ~1.429MHz.

### Available flash read/write functions
- begin(): Initialize the chip and save the jedec-id.
- read8(n): Reads 8-bit value from address n
- read16(n): "" 16-bit ""
- read32(n): "" 32-bit ""
- writeBuffer(addr, *buff, len): Writes *buff to addr + len.
- getUsedMemory(): Searches for a chunk of 0xffffffff and returns used bytes. Only works contiguously and if 0xffffffff isn't in the used data!
- eraseChip(): Erases entire chip.
- eraseSector(n): Erases a 4k sector at sectornumber n.
- eraseBlock32(n): Erases a 32k block at blocknumber n.
- eraseBlock64(n): Erases a 64k block at blocknumber n.
- getJEDECID(): Returns the JEDEC ID in the format (0xEF7018): Manufacturer ID(0xEF), Memory Type(0x70), Capacity(0x18)
