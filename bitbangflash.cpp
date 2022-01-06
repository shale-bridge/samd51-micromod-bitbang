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


#include "bitbangflash.h"

bool BitBangFlash::begin(unsigned long CLK_Delay)
{
	delayUs = CLK_Delay;	// Only used to slow datalines for debugging

	uint8_t* pinCfg = (uint8_t*)&REG_PORT_PINCFG0;
	pinCfg[MISO_PORT_] = (uint8_t)(PORT_PINCFG_INEN | PORT_PINCFG_PULLEN);
	SET_INPUT(PORTA) = MISO_;
	SET_HIGH(PORTA) = MISO_;	// Enable pullup

	SET_OUTPUT(PORTA) = FCS_ + MOSI_ + CLK_;
	SET_OUTPUT(PORTB) = WP_ + HOLD_;
	SET_HIGH(PORTB) = WP_ + HOLD_;
	SET_LOW(PORTA) = CLK_;

	readCommand(SFLASH_CMD_READ_JEDEC_ID, jedec_id, 4);

	while (readStatus() & 0x01);	// WIP (Write-in-progress) should be low.
	while (readStatus2() & 0x80);	// The suspended write/erase bit should be low.

	runCommand(SFLASH_CMD_ENABLE_RESET);
	runCommand(SFLASH_CMD_RESET);

	delayMicroseconds(30);
	waitUntilReady();

	return true;
}

uint32_t BitBangFlash::getJEDECID(void) 
{
	return ((uint32_t)jedec_id[0]) << 16 | jedec_id[1] << 8 | jedec_id[2];
}

void BitBangFlash::waitUntilReady(void) 
{
	// both WIP and WREN bit should be clear
	while (readStatus() & 0x03)
		yield();
}

void BitBangFlash::readCommand(FlashCommands command, uint8_t* response, uint32_t len) 
{
	SET_LOW(PORTA) = FCS_;
	transfer(command);
	while (len--)
		*response++ = transfer(0xFF);
	SET_HIGH(PORTA) = FCS_;
}

uint8_t BitBangFlash::readStatus(void) 
{
	uint8_t status;
	readCommand(SFLASH_CMD_READ_STATUS, &status, 1);
	return status;
}

uint8_t BitBangFlash::readStatus2(void) 
{
	uint8_t status;
	readCommand(SFLASH_CMD_READ_STATUS2, &status, 1);
	return status;
}

uint8_t BitBangFlash::read8(uint32_t addr) 
{
	uint8_t ret;
	return readBuffer(addr, &ret, sizeof(ret)) ? ret : 0xff;
}

uint16_t BitBangFlash::read16(uint32_t addr) 
{
	uint16_t ret;
	return readBuffer(addr, (uint8_t*)&ret, sizeof(ret)) ? ret : 0xffff;
}

uint32_t BitBangFlash::read32(uint32_t addr) 
{
	uint32_t ret;
	return readBuffer(addr, (uint8_t*)&ret, sizeof(ret)) ? ret : 0xffffffff;
}

uint32_t BitBangFlash::readBuffer(uint32_t address, uint8_t* buffer, uint32_t len)
{
	waitUntilReady();
	bool const rc = readMemory(address, buffer, len);
	return rc ? len : 0;
}

bool BitBangFlash::readMemory(uint32_t addr, uint8_t* data, uint32_t len)
{
	SET_LOW(PORTA) = FCS_;

	uint8_t cmd_with_addr[6] = { SFLASH_CMD_FAST_READ };
	fillAddress(cmd_with_addr + 1, addr);

	// Fast Read has 1 extra dummy byte
	uint8_t const cmd_len =	1 + _addr_len + 1;

	transfer(cmd_with_addr, cmd_len);
	transfer(data, len);

	SET_HIGH(PORTA) = FCS_;

	return true;
}

void BitBangFlash::fillAddress(uint8_t* buf, uint32_t addr) 
{
	*buf++ = (addr >> 16) & 0xFF;
	*buf++ = (addr >> 8) & 0xFF;
	*buf++ = addr & 0xFF;
}

bool BitBangFlash::writeMemory(uint32_t addr, uint8_t const* data, uint32_t len) 
{
	SET_LOW(PORTA) = FCS_;

	uint8_t cmd_with_addr[5] = { SFLASH_CMD_PAGE_PROGRAM };
	fillAddress(cmd_with_addr + 1, addr);
	
	transfer(cmd_with_addr, 1 + _addr_len);

	while (len--)
		transfer(*data++);

	SET_HIGH(PORTA) = FCS_;

	return true;
}

uint32_t BitBangFlash::writeBuffer(uint32_t address, uint8_t const* buffer, uint32_t len)
{
	uint32_t remain = len;

	while (remain) 
	{
		waitUntilReady();
		writeEnable();
	
		uint32_t const leftOnPage =	SFLASH_PAGE_SIZE - (address & (SFLASH_PAGE_SIZE - 1));
		uint32_t const toWrite = min(remain, leftOnPage);
	
		if (!writeMemory(address, buffer, toWrite))
			break;
	
		remain -= toWrite;
		buffer += toWrite;
		address += toWrite;
	}
	len -= remain;
	
	return len;
}

/* 
 * Returns the amount of contiguous memory that is currently used in bytes. 
 * NOTE: This only works if there aren't any chunks of 0xffffffff in your
 *	own data!
 */
uint32_t BitBangFlash::getUsedMemory(void)
{
	uint32_t usedMemory = 0;
	for (uint32_t x = 0; x != 0xffffffff; usedMemory++)
		x = read32(usedMemory);
	return usedMemory - 1;
}

/* Erase whole chip. NOTE: This can take up to 40 seconds or longer (W25Q128 datasheet) */
bool BitBangFlash::eraseChip(void)
{
	waitUntilReady();
	writeEnable();

	bool const ret = runCommand(SFLASH_CMD_ERASE_CHIP);
	return ret;
}

/* Erase 4k sector */
bool BitBangFlash::eraseSector(uint32_t sectorNumber)
{
	waitUntilReady();
	writeEnable();

	bool const ret = eraseCommand(SFLASH_CMD_ERASE_SECTOR, sectorNumber * SFLASH_SECTOR_SIZE);
	return ret;
}

/* Erase 32k block */
bool BitBangFlash::eraseBlock32(uint32_t blockNumber) 
{
	waitUntilReady();
	writeEnable();

	bool const ret = eraseCommand(SFLASH_CMD_ERASE_BLOCK_32K, blockNumber * SFLASH_BLOCK_SIZE32);
	return ret;
}

/* Erase 64k block */
bool BitBangFlash::eraseBlock64(uint32_t blockNumber) 
{
	waitUntilReady();
	writeEnable();

	bool const ret = eraseCommand(SFLASH_CMD_ERASE_BLOCK_64K, blockNumber * SFLASH_BLOCK_SIZE64);
	return ret;
}

bool BitBangFlash::eraseCommand(FlashCommands command, uint32_t addr) 
{
	SET_LOW(PORTA) = FCS_;

	uint8_t cmd_with_addr[5] = { command };
	fillAddress(cmd_with_addr + 1, addr);

	transfer(cmd_with_addr, 1 + _addr_len);

	SET_HIGH(PORTA) = FCS_;

	return true;
}

bool BitBangFlash::writeEnable(void) 
{
	return runCommand(SFLASH_CMD_WRITE_ENABLE);
}

bool BitBangFlash::runCommand(FlashCommands command) 
{
	SET_LOW(PORTA) = FCS_;
	transfer(command);
	SET_HIGH(PORTA) = FCS_;
	return true;
}

uint8_t BitBangFlash::transfer(uint8_t data)
{
	for (uint_fast8_t bit = 8; bit > 0; --bit)
	{
		((data & 0x80) ? SET_HIGH(PORTA) : SET_LOW(PORTA)) = MOSI_;
	  
		data <<= 1;
		data |= (READ_PORT(PORTA) & MISO_) != 0;
		
		REG_PORT_OUTTGL0 = CLK_;		//4.789MHz, instead of ~1.6MHz
		REG_PORT_OUTTGL0 = CLK_;
	}
	return data;
} 

void BitBangFlash::transfer(uint8_t* buf, size_t count)
{
	uint8_t* buffer = buf;
	for (size_t i = count; i > 0; --i)
	{
		*buffer = transfer(*buffer);
		buffer++;
	}
}