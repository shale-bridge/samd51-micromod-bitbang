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
 * Estimated clock speeds are around ~1.429MHz from my testing.
 *
 *  ============================================================================
 */


#include "bitbangflash.h"


bool BitBangFlash::begin(unsigned long CLK_Delay)
{
	delayUs = CLK_Delay;	// Only used to slow datalines for debugging

	PORT->Group[PORTA].PINCFG[g_APinDescription[49].ulPin].reg = (uint8_t)(PORT_PINCFG_INEN | PORT_PINCFG_PULLEN);
	SET_INPUT(PORTA) = MISO_;
	SET_HIGH(PORTA) = MISO_;	// Enable pullup

	SET_OUTPUT(PORTA) = FCS_ + MOSI_ + CLK_;
	SET_OUTPUT(PORTB) = WP_ + HOLD_;
	SET_HIGH(PORTB) = WP_ + HOLD_;

	readCommand(SFLASH_CMD_READ_JEDEC_ID, jedec_id, 4);
	// Serial.println(
	// 	((uint32_t)jedec_id[0]) << 16 | jedec_id[1] << 8 | jedec_id[2], HEX);

	while (readStatus() & 0x01);
	while (readStatus2() & 0x80);

	runCommand(SFLASH_CMD_ENABLE_RESET);
	runCommand(SFLASH_CMD_RESET);

	delayMicroseconds(30);
	waitUntilReady();

	return true;
}

void BitBangFlash::waitUntilReady(void) 
{
	// both WIP and WREN bit should be clear
	while (readStatus() & 0x03)
		yield();
}

bool BitBangFlash::readCommand(uint8_t command, uint8_t* response, uint32_t len) {
	SET_LOW(PORTA) = FCS_;
	transfer(command);
	while (len--)
		*response++ = transfer(0xFF);
	SET_HIGH(PORTA) = FCS_;
	return true;
}

uint8_t BitBangFlash::readStatus(void) {
	uint8_t status;
	readCommand(SFLASH_CMD_READ_STATUS, &status, 1);
	return status;
}

uint8_t BitBangFlash::readStatus2(void) {
	uint8_t status;
	readCommand(SFLASH_CMD_READ_STATUS2, &status, 1);
	return status;
}

uint8_t BitBangFlash::read8(uint32_t addr) {
	uint8_t ret;
	return readBuffer(addr, &ret, sizeof(ret)) ? ret : 0xff;
}

uint16_t BitBangFlash::read16(uint32_t addr) {
	uint16_t ret;
	return readBuffer(addr, (uint8_t*)&ret, sizeof(ret)) ? ret : 0xffff;
}

uint32_t BitBangFlash::read32(uint32_t addr) {
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
	uint8_t const cmd_len =	1 + _addr_len + (SFLASH_CMD_FAST_READ == SFLASH_CMD_FAST_READ ? 1 : 0);

	transfer(cmd_with_addr, cmd_len);
	transfer(data, len);

	SET_HIGH(PORTA) = FCS_;

	return true;
}

void BitBangFlash::fillAddress(uint8_t* buf, uint32_t addr) {
	switch (_addr_len) {
	case 4:
		*buf++ = (addr >> 24) & 0xFF;
	case 3:
		*buf++ = (addr >> 16) & 0xFF;
	case 2:
	default:
		*buf++ = (addr >> 8) & 0xFF;
		*buf++ = addr & 0xFF;
	}
}

bool BitBangFlash::writeMemory(uint32_t addr, uint8_t const* data, uint32_t len) {
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

bool BitBangFlash::eraseChip(void)
{
	waitUntilReady();
	writeEnable();

	bool const ret = runCommand(SFLASH_CMD_ERASE_CHIP);
	return ret;
}

bool BitBangFlash::eraseBlock(uint32_t blockNumber) {
	waitUntilReady();
	writeEnable();

	bool const ret = eraseCommand(SFLASH_CMD_ERASE_BLOCK, blockNumber * SFLASH_BLOCK_SIZE);
	return ret;
}

bool BitBangFlash::eraseCommand(uint8_t command, uint32_t addr) {
	SET_LOW(PORTA) = FCS_;

	uint8_t cmd_with_addr[5] = { command };
	fillAddress(cmd_with_addr + 1, addr);

	transfer(cmd_with_addr, 1 + _addr_len);

	SET_HIGH(PORTA) = FCS_;

	return true;
}

bool BitBangFlash::writeEnable(void) {
	return runCommand(SFLASH_CMD_WRITE_ENABLE);
}

bool BitBangFlash::runCommand(uint8_t command) {
	SET_LOW(PORTA) = FCS_;
	transfer(command);
	SET_HIGH(PORTA) = FCS_;
	return true;
}

uint8_t BitBangFlash::transfer(uint8_t data)
{
	for (uint8_t bit = 0; bit < 8; bit++)
	{
		((data & 0x80) ? SET_HIGH(PORTA) : SET_LOW(PORTA)) = MOSI_;
	  
		data <<= 1;
		data |= (READ_PORT(PORTA) & MISO_) != 0;

		SET_HIGH(PORTA) = CLK_;
		delayMicroseconds(delayUs);
		SET_LOW(PORTA) = CLK_;
		delayMicroseconds(delayUs);
	}
	return data;
} 

void BitBangFlash::transfer(void* buf, size_t count)
{
	uint8_t* buffer = reinterpret_cast<uint8_t*>(buf);
	for (size_t i = 0; i < count; i++) 
	{
		*buffer = transfer(*buffer);
		buffer++;
	}
}