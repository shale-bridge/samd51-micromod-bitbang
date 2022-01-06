#pragma once
/*
 * ============================================================================
 * $ Bit-bang SPI flash library. Specialized for the SAMD51 MicroMod board. $
 *
 * SAMD51 MicroMod board v1.2 has a layout error that
 * swaps the MISO/MOSI pins going to the external flash.
 * Those pins can't be re-muxed internally on the chip,
 * so SPI is unavailable for use. This is built
 * off of the Adafruit SPIFlash library and "manually"
 * clocks and sends/receives data.
 *
 * Estimated clock speeds are around ~1.429MHz. This can be doubled by removing
 * the calls to delayMicroseconds(delayUs). delayUs is 0 by default.
 * 
 * - SIZE:			16MB (16,777,216)
 * - 4k SECTORS:	4,096
 * - 32k BLOCKS:	512
 * - 64k BLOCKS:	256
 *
 *  ============================================================================
 */

#include <Arduino.h>

enum FlashCommands 
{
	SFLASH_CMD_READ = 0x03,      // Single Read
	SFLASH_CMD_FAST_READ = 0x0B, // Fast Read
	SFLASH_CMD_QUAD_READ = 0x6B, // 1 line address, 4 line data

	SFLASH_CMD_READ_JEDEC_ID = 0x9f,

	SFLASH_CMD_PAGE_PROGRAM = 0x02,
	SFLASH_CMD_QUAD_PAGE_PROGRAM = 0x32, // 1 line address, 4 line data

	SFLASH_CMD_READ_STATUS = 0x05,
	SFLASH_CMD_READ_STATUS2 = 0x35,

	SFLASH_CMD_WRITE_STATUS = 0x01,
	SFLASH_CMD_WRITE_STATUS2 = 0x31,

	SFLASH_CMD_ENABLE_RESET = 0x66,
	SFLASH_CMD_RESET = 0x99,

	SFLASH_CMD_WRITE_ENABLE = 0x06,
	SFLASH_CMD_WRITE_DISABLE = 0x04,

	SFLASH_CMD_ERASE_SECTOR = 0x20,
	SFLASH_CMD_ERASE_BLOCK_32K = 0x52,
	SFLASH_CMD_ERASE_BLOCK_64K = 0xD8,
	SFLASH_CMD_ERASE_CHIP = 0xC7,

	SFLASH_CMD_4_BYTE_ADDR = 0xB7,
	SFLASH_CMD_3_BYTE_ADDR = 0xE9,
};

enum MemorySize 
{
    SFLASH_BLOCK_SIZE32 = 32 * 1024UL,
	SFLASH_BLOCK_SIZE64 = 64 * 1024UL,
	SFLASH_SECTOR_SIZE = 4 * 1024,
	SFLASH_PAGE_SIZE = 256,
};

/* Port Macros */
#define SET_HIGH(x)     (PORT->Group[x].OUTSET.reg)
#define SET_LOW(x)      (PORT->Group[x].OUTCLR.reg)
#define SET_OUTPUT(x)   (PORT->Group[x].DIRSET.reg)
#define SET_INPUT(x)	(PORT->Group[x].DIRCLR.reg)
#define READ_PORT(x)	(PORT->Group[x].IN.reg)

#define CLK_	PORT_PA08
#define MISO_	PORT_PA09
#define MOSI_	PORT_PA10
#define FCS_	PORT_PA11
#define	WP_		PORT_PB22
#define HOLD_	PORT_PB23
#define MISO_PIN_	49
#define MISO_PORT_	9

class BitBangFlash
{
public:
	// delay for clock being high
	unsigned long	delayUs = 0;

	uint8_t	jedec_id[4];
	uint8_t	_addr_len = 3;	// 24 bits (8 * 3)

	/* User functions */
	bool	begin(unsigned long CLK_Delay = 0);
	uint8_t	read8(uint32_t addr);
	uint16_t	read16(uint32_t addr);
	uint32_t	read32(uint32_t addr);
	uint32_t	writeBuffer(uint32_t addr, uint8_t const* buffer, uint32_t len);
	bool	eraseChip(void);
	bool	eraseSector(uint32_t sectorNum);
	bool	eraseBlock32(uint32_t blockNum);
	bool	eraseBlock64(uint32_t blockNum);
	uint32_t	getJEDECID(void);
	uint32_t	getUsedMemory(void);
	
private:
	/* Internal functions */
	inline bool	eraseCommand(FlashCommands command, uint32_t addr);
	inline void	waitUntilReady(void);
	inline uint8_t	readStatus(void);
	inline uint8_t	readStatus2(void);
	inline bool	readMemory(uint32_t addr, uint8_t* data, uint32_t len);
	inline uint32_t	readBuffer(uint32_t addr, uint8_t* buffer, uint32_t len);
	inline void	fillAddress(uint8_t* buffer, uint32_t addr);
	inline bool	writeMemory(uint32_t addr, uint8_t const* data, uint32_t len);
	inline bool	writeEnable(void);
	inline void	readCommand(FlashCommands command, uint8_t* response, uint32_t len);
	inline bool	runCommand(FlashCommands command);
	inline uint8_t	transfer(uint8_t data);
	inline void	transfer(uint8_t* buffer, size_t count);
};
