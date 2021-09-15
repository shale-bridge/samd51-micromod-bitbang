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
	delayUs = CLK_Delay;

	PORT->Group[PORTA].PINCFG[g_APinDescription[49].ulPin].reg = (uint8_t)(PORT_PINCFG_INEN | PORT_PINCFG_PULLEN);
	SET_INPUT(PORTA) = MISO_;
	SET_HIGH(PORTA) = MISO_;	// Enable pullup

	SET_OUTPUT(PORTA) = FCS_ + MOSI_ + CLK_;
	SET_OUTPUT(PORTB) = WP_ + HOLD_;
	SET_HIGH(PORTB) = WP_ + HOLD_;

	ReadCommand(SFLASH_CMD_READ_JEDEC_ID, jedec_id, 4);
	// Serial.println(
	// 	((uint32_t)jedec_id[0]) << 16 | jedec_id[1] << 8 | jedec_id[2], HEX);

	while (ReadStatus() & 0x01);
	while (ReadStatus2() & 0x80);

	RunCommand(SFLASH_CMD_ENABLE_RESET);
	RunCommand(SFLASH_CMD_RESET);

	delayMicroseconds(30);
	WaitUntilReady();

	return true;
}

void BitBangFlash::WaitUntilReady(void) 
{
	// both WIP and WREN bit should be clear
	while (ReadStatus() & 0x03)
		yield();
}

bool BitBangFlash::ReadCommand(uint8_t command, uint8_t* response, uint32_t len) {
	SET_LOW(PORTA) = FCS_;
	Transfer(command);
	while (len--)
		*response++ = Transfer(0xFF);
	SET_HIGH(PORTA) = FCS_;
	return true;
}

uint8_t BitBangFlash::ReadStatus(void) {
	uint8_t status;
	ReadCommand(SFLASH_CMD_READ_STATUS, &status, 1);
	return status;
}

uint8_t BitBangFlash::ReadStatus2(void) {
	uint8_t status;
	ReadCommand(SFLASH_CMD_READ_STATUS2, &status, 1);
	return status;
}

uint8_t BitBangFlash::read8(uint32_t addr) {
	uint8_t ret;
	return ReadBuffer(addr, &ret, sizeof(ret)) ? ret : 0xff;
}

uint16_t BitBangFlash::Read16(uint32_t addr) {
	uint16_t ret;
	return ReadBuffer(addr, (uint8_t*)&ret, sizeof(ret)) ? ret : 0xffff;
}

uint32_t BitBangFlash::Read32(uint32_t addr) {
	uint32_t ret;
	return ReadBuffer(addr, (uint8_t*)&ret, sizeof(ret)) ? ret : 0xffffffff;
}

uint32_t BitBangFlash::ReadBuffer(uint32_t address, uint8_t* buffer, uint32_t len)
{
	WaitUntilReady();
	bool const rc = ReadMemory(address, buffer, len);
	return rc ? len : 0;
}

bool BitBangFlash::ReadMemory(uint32_t addr, uint8_t* data, uint32_t len)
{
	SET_LOW(PORTA) = FCS_;

	uint8_t cmd_with_addr[6] = { SFLASH_CMD_FAST_READ };
	FillAddress(cmd_with_addr + 1, addr);

	// Fast Read has 1 extra dummy byte
	uint8_t const cmd_len =	1 + _addr_len + (SFLASH_CMD_FAST_READ == SFLASH_CMD_FAST_READ ? 1 : 0);

	Transfer(cmd_with_addr, cmd_len);
	Transfer(data, len);

	SET_HIGH(PORTA) = FCS_;

	return true;
}

void BitBangFlash::FillAddress(uint8_t* buf, uint32_t addr) {
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

bool BitBangFlash::WriteMemory(uint32_t addr, uint8_t const* data, uint32_t len) {
	SET_LOW(PORTA) = FCS_;

	uint8_t cmd_with_addr[5] = { SFLASH_CMD_PAGE_PROGRAM };
	FillAddress(cmd_with_addr + 1, addr);
	
	Transfer(cmd_with_addr, 1 + _addr_len);

	while (len--)
		Transfer(*data++);

	SET_HIGH(PORTA) = FCS_;

	return true;
}

uint32_t BitBangFlash::writeBuffer(uint32_t address, uint8_t const* buffer, uint32_t len)
{
	uint32_t remain = len;

	while (remain) 
	{
		WaitUntilReady();
		WriteEnable();
	
		uint32_t const leftOnPage =	SFLASH_PAGE_SIZE - (address & (SFLASH_PAGE_SIZE - 1));
		uint32_t const toWrite = min(remain, leftOnPage);
	
		if (!WriteMemory(address, buffer, toWrite))
			break;
	
		remain -= toWrite;
		buffer += toWrite;
		address += toWrite;
	}
	len -= remain;
	
	return len;
}

bool BitBangFlash::EraseChip(void)
{
	WaitUntilReady();
	WriteEnable();

	bool const ret = RunCommand(SFLASH_CMD_ERASE_CHIP);
	return ret;
}

bool BitBangFlash::eraseBlock(uint32_t blockNumber) {
	WaitUntilReady();
	WriteEnable();

	bool const ret = EraseCommand(SFLASH_CMD_ERASE_BLOCK, blockNumber * SFLASH_BLOCK_SIZE);
	return ret;
}

bool BitBangFlash::EraseCommand(uint8_t command, uint32_t addr) {
	SET_LOW(PORTA) = FCS_;

	uint8_t cmd_with_addr[5] = { command };
	FillAddress(cmd_with_addr + 1, addr);

	Transfer(cmd_with_addr, 1 + _addr_len);

	SET_HIGH(PORTA) = FCS_;

	return true;
}

bool BitBangFlash::WriteEnable(void) {
	return RunCommand(SFLASH_CMD_WRITE_ENABLE);
}

bool BitBangFlash::RunCommand(uint8_t command) {
	SET_LOW(PORTA) = FCS_;
	Transfer(command);
	SET_HIGH(PORTA) = FCS_;
	return true;
}

uint8_t BitBangFlash::Transfer(uint8_t data)
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

void BitBangFlash::Transfer(void* buf, size_t count)
{
	uint8_t* buffer = reinterpret_cast<uint8_t*>(buf);
	for (size_t i = 0; i < count; i++) 
	{
		*buffer = Transfer(*buffer);
		buffer++;
	}
}