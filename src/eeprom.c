/***
	eeprom.c:	minimal eeprom interface

	The author can be reached at: bill@bitlash.net

	Copyright (C) 2008-2012 Bill Roy

	Permission is hereby granted, free of charge, to any person
	obtaining a copy of this software and associated documentation
	files (the "Software"), to deal in the Software without
	restriction, including without limitation the rights to use,
	copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following
	conditions:
	
	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.

***/

/***
 MSP430 port  (C) 2014 smarpl.com

 Flash Erase/Write code from MspFlash library for MSP430 Energia (c) 2012 Peter Brier.

 ***/


#include "bitlash.h"

#if defined(AVR_BUILD)

#include "avr/eeprom.h"
void eewrite(int addr, uint8_t value) { eeprom_write_byte((unsigned char *) addr, value); }
uint8_t eeread(int addr) { return eeprom_read_byte((unsigned char *) addr); }

#elif defined(ARM_BUILD)

// A little fake eeprom for ARM testing
char virtual_eeprom[E2END];

void eeinit(void) {
	for (int i=0; i<E2END; i++) virtual_eeprom[i] = 255;
}

void eewrite(int addr, uint8_t value) { virtual_eeprom[addr] = value; }
uint8_t eeread(int addr) { return virtual_eeprom[addr]; }


#elif defined(MSP430_BUILD)

#if defined(MSP430_FRAM)


void eewrite(int addr, uint8_t value) {
 	*(uint8_t *) (addr + FRAM_USERDATA_START) = value;
}

uint8_t eeread(int addr) {
 	return *(uint8_t *) (addr + FRAM_USERDATA_START);
}

#else

#ifdef __MSP430_HAS_FLASH2__
#define INFO_SIZE 64 // information flah sizes (SEG_A to SEG_D)
#else
#define INFO_SIZE 128 // information flah sizes (SEG_A to SEG_D)
#endif

#ifdef __MSP430_HAS_FLASH2__
// segment addresses of 64 byte segments
#define SEGMENT_D ((unsigned char*)0x1000)
#define SEGMENT_C ((unsigned char*)0x1000+64)
#define SEGMENT_B ((unsigned char*)0x1000+128)
#else
#define SEGMENT_D ((unsigned char*)0x1800)
#define SEGMENT_C ((unsigned char*)0x1800+128)
#define SEGMENT_B ((unsigned char*)0x1800+256)
#endif

#define FLASHCLOCK FSSEL1+((F_CPU/400000L) & 63); // SCLK


void flasherase(unsigned char *flash)
{
	  disableWatchDog();        // Disable WDT
	#ifdef __MSP430_HAS_FLASH2__
	  FCTL2 = FWKEY+FLASHCLOCK; // SMCLK/2
	  FCTL3 = FWKEY;            // Clear LOCK
	  FCTL1 = FWKEY+ERASE;      //Enable segment erase
	  *flash = 0;               // Dummy write, erase Segment
	  FCTL3 = FWKEY+LOCK;       // Done, set LOCK
	#else
	  while (FCTL3 & BUSY)      // Wait for till busy flag is 0
	  {
	    __no_operation();
	  }

	  FCTL3 = FWKEY;            // Clear Lock bit
	  FCTL1 = FWKEY+ERASE;      // Set Erase bit
	  *flash = 0;               // Dummy write, erase Segment

	  while (FCTL3 & BUSY)      // Wait for till busy flag is 0
	  {
	    __no_operation();
	  }

	  FCTL3 = FWKEY+LOCK;       // Set LOCK bit
	#endif
	  enableWatchDog();         // Enable WDT
}

void flashwrite(unsigned char *flash, unsigned char *src, int len)
{
 disableWatchDog();        // Disable WDT
#ifdef __MSP430_HAS_FLASH2__
 FCTL2 = FWKEY+FLASHCLOCK; // SMCLK/2
 FCTL3 = FWKEY;            // Clear LOCK
 FCTL1 = FWKEY+WRT;        // Enable write
 while(len--)              // Copy data
   *(flash++) = *(src++);
 FCTL1 = FWKEY;            //Done. Clear WRT
 FCTL3 = FWKEY+LOCK;       // Set LOCK
#else
 FCTL3 = FWKEY;            // Clear Lock bit
 FCTL1 = FWKEY+WRT;        // Set Erase bit
 while(len--)              // Copy data
   *(flash++) = *(src++);
 FCTL1 = FWKEY;            // Done. Clear WRT
 FCTL3 = FWKEY+LOCK;       // Set LOCK
#endif
 enableWatchDog();         // Enable WDT
}


#if defined(MSP430_FLASH_CACHE)

uint8_t flash_cache[E2END];

void eeinit(void) {
	for (int i = 0; i < E2END; i++) {
		flash_cache[i] = *(SEGMENT_D + i);
	}
}

void eewrite(int addr, uint8_t value) {
	flash_cache[addr] = value;
}

uint8_t eeread(int addr) {
	return flash_cache[addr];
}

int need_erase(unsigned char * segment, unsigned char * cache) {
	for (int i = 0; i < INFO_SIZE; i++) {
		if (~segment[i] & cache[i])
			return 1;
	}
	return 0;
}

void flushcache() {
	if (need_erase(SEGMENT_D, flash_cache)) {
		flasherase(SEGMENT_D);
	}
	if (need_erase(SEGMENT_C, flash_cache)) {
		flasherase(SEGMENT_C);
	}

	if (need_erase(SEGMENT_B, flash_cache)) {
		flasherase(SEGMENT_B);
	}
	flashwrite(SEGMENT_D, flash_cache, INFO_SIZE);
	flashwrite(SEGMENT_C, flash_cache + INFO_SIZE, INFO_SIZE);
	flashwrite(SEGMENT_B, flash_cache + INFO_SIZE + INFO_SIZE, INFO_SIZE);
}

#else

void eerase(void) {
	flasherase(SEGMENT_D);
	flasherase(SEGMENT_C);
	flasherase(SEGMENT_B);
}

void eewrite(int addr, uint8_t value) {
	flashwrite(SEGMENT_D + addr, &value, 1);
}


uint8_t eeread(int addr) {
	return *(SEGMENT_D+addr);
}

#endif  // MSP430_FLASH_CACHE

#endif  // MSP430_FRAM

#endif

