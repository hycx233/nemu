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

/* Page table translation: linear address -> physical address
 * IA-32 uses two-level page table:
 * Linear address format (32-bit):
 *   [31:22] - Page Directory Index (10 bits)
 *   [21:12] - Page Table Index (10 bits)
 *   [11:0]  - Page Offset (12 bits)
 */
static hwaddr_t page_translate(lnaddr_t addr) {
	/* Get page directory base from CR3 */
	uint32_t pdir_base = cpu.cr3.page_directory_base << 12;
	
	/* Extract page directory index (bits 31:22) */
	uint32_t pdir_idx = (addr >> 22) & 0x3ff;
	
	/* Read page directory entry */
	PDE pde;
	pde.val = hwaddr_read(pdir_base + pdir_idx * 4, 4);
	
	/* Check if page directory entry is present */
	if (!pde.present) {
		fprintf(stderr, "Page fault: linear address = 0x%08x, pdir_base = 0x%08x, pdir_idx = %u\n", addr, pdir_base, pdir_idx);
	}
	assert(pde.present);
	
	/* Get page table base from PDE */
	uint32_t ptable_base = pde.page_frame << 12;
	
	/* Extract page table index (bits 21:12) */
	uint32_t ptable_idx = (addr >> 12) & 0x3ff;
	
	/* Read page table entry */
	PTE pte;
	pte.val = hwaddr_read(ptable_base + ptable_idx * 4, 4);
	
	/* Check if page table entry is present */
	assert(pte.present);
	
	/* Get physical page frame from PTE */
	uint32_t page_frame = pte.page_frame << 12;
	
	/* Extract page offset (bits 11:0) */
	uint32_t offset = addr & 0xfff;
	
	/* Physical address = page frame + offset */
	return page_frame | offset;
}

uint32_t lnaddr_read(lnaddr_t addr, size_t len) {
	/* Check if paging is enabled */
	if (cpu.cr0.paging) {
		/* Handle cross-page access */
		if ((addr & 0xfff) + len > 0x1000) {
			/* Data crosses page boundary, need to read byte by byte */
			uint32_t result = 0;
			size_t i;
			for (i = 0; i < len; i++) {
				hwaddr_t hwaddr = page_translate(addr + i);
				uint32_t byte = hwaddr_read(hwaddr, 1);
				result |= (byte << (i * 8));
			}
			return result;
		}
		hwaddr_t hwaddr = page_translate(addr);
		return hwaddr_read(hwaddr, len);
	}
	return hwaddr_read(addr, len);
}

void lnaddr_write(lnaddr_t addr, size_t len, uint32_t data) {
	/* Check if paging is enabled */
	if (cpu.cr0.paging) {
		/* Handle cross-page access */
		if ((addr & 0xfff) + len > 0x1000) {
			/* Data crosses page boundary, need to write byte by byte */
			size_t i;
			for (i = 0; i < len; i++) {
				hwaddr_t hwaddr = page_translate(addr + i);
				hwaddr_write(hwaddr, 1, (data >> (i * 8)) & 0xff);
			}
			return;
		}
		hwaddr_t hwaddr = page_translate(addr);
		hwaddr_write(hwaddr, len, data);
		return;
	}
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

