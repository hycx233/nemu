#include "common.h"
#include "memory/cache.h"
#include "cpu/reg.h"
#include "../../lib-common/x86-inc/mmu.h"

/* current segment register index used for memory access */
int current_sreg = R_DS;

/* Memory accessing interfaces */

uint32_t hwaddr_read(hwaddr_t addr, size_t len) {
	return cache_read(addr, len) & (~0u >> ((4 - len) << 3));
}

void hwaddr_write(hwaddr_t addr, size_t len, uint32_t data) {
	cache_write(addr, len, data);
}

uint32_t lnaddr_read(lnaddr_t addr, size_t len) {
	return hwaddr_read(addr, len);
}

void lnaddr_write(lnaddr_t addr, size_t len, uint32_t data) {
	hwaddr_write(addr, len, data);
}

/* Load segment descriptor from GDT and get base address */
static uint32_t seg_translate(swaddr_t addr, int sreg) {
	/* Get segment selector */
	uint16_t selector = cpu.sreg[sreg].val;
	
	/* Get segment descriptor index (bits 15:3) */
	uint32_t index = selector >> 3;
	
	/* Read segment descriptor from GDT */
	/* Each descriptor is 8 bytes */
	uint32_t desc_addr = cpu.gdtr.base + index * 8;
	
	/* Read the 8-byte segment descriptor */
	uint32_t desc_low = lnaddr_read(desc_addr, 4);
	uint32_t desc_high = lnaddr_read(desc_addr + 4, 4);
	
	/* Parse the descriptor to get base address */
	/* Base address is scattered across 3 fields:
	 * base_15_0:  bits 16-31 of desc_low
	 * base_23_16: bits 0-7 of desc_high  
	 * base_31_24: bits 24-31 of desc_high
	 */
	uint32_t base = ((desc_low >> 16) & 0xffff) |     /* base_15_0 */
	                ((desc_high & 0xff) << 16) |       /* base_23_16 */
	                (desc_high & 0xff000000);          /* base_31_24 */
	
	/* Linear address = base + offset */
	return base + addr;
}

uint32_t swaddr_read(swaddr_t addr, size_t len) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	lnaddr_t lnaddr;
	
	/* Check if protected mode is enabled */
	if (cpu.cr0.protect_enable) {
		lnaddr = seg_translate(addr, current_sreg);
	} else {
		lnaddr = addr;
	}
	
	return lnaddr_read(lnaddr, len);
}

void swaddr_write(swaddr_t addr, size_t len, uint32_t data) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	lnaddr_t lnaddr;
	
	/* Check if protected mode is enabled */
	if (cpu.cr0.protect_enable) {
		lnaddr = seg_translate(addr, current_sreg);
	} else {
		lnaddr = addr;
	}
	
	lnaddr_write(lnaddr, len, data);
}

