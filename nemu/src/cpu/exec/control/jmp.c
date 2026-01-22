#include "cpu/exec/helper.h"
#include "cpu/decode/modrm.h"
#include "memory/memory.h"

#define DATA_BYTE 1
#include "jmp-template.h"
#undef DATA_BYTE

#define DATA_BYTE 4
#include "jmp-template.h"
#undef DATA_BYTE

/* Helper function to load segment descriptor cache */
static void load_sreg(int sreg, uint16_t selector) {
	cpu.sreg[sreg].val = selector;
	
	/* Get segment descriptor index (bits 15:3) */
	uint32_t index = selector >> 3;
	
	/* Read segment descriptor from GDT */
	uint32_t desc_addr = cpu.gdtr.base + index * 8;
	
	/* Read the 8-byte segment descriptor */
	uint32_t desc_low = lnaddr_read(desc_addr, 4);
	uint32_t desc_high = lnaddr_read(desc_addr + 4, 4);
	
	/* Parse the descriptor to get base address */
	uint32_t base = ((desc_low >> 16) & 0xffff) |     /* base_15_0 */
	                ((desc_high & 0xff) << 16) |       /* base_23_16 */
	                (desc_high & 0xff000000);          /* base_31_24 */
	
	/* Parse the descriptor to get limit */
	uint32_t limit = (desc_low & 0xffff) |            /* limit_15_0 */
	                 ((desc_high & 0xf0000));          /* limit_19_16 */
	
	/* Check granularity bit */
	if (desc_high & (1 << 23)) {
		/* 4KB granularity */
		limit = (limit << 12) | 0xfff;
	}
	
	cpu.sreg[sreg].base = base;
	cpu.sreg[sreg].limit = limit;
}

/* ljmp ptr16:32 - far jump direct
 * Opcode: EA (cd cw/cd)
 * Format: EA offset segment
 * offset: 4 bytes (32-bit mode)
 * segment: 2 bytes
 */
make_helper(ljmp) {
	/* Read 4-byte offset */
	uint32_t offset = instr_fetch(eip + 1, 4);
	/* Read 2-byte segment selector */
	uint16_t selector = instr_fetch(eip + 5, 2);
	
	/* Load CS segment register */
	load_sreg(R_CS, selector);
	
	/* Set EIP to target offset, accounting for instruction length */
	cpu.eip = offset - 7;
	
	print_asm("ljmp $0x%x,$0x%x", selector, offset);
	return 7;
}
