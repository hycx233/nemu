#include "cpu/exec/helper.h"
#include "cpu/decode/modrm.h"
#include "memory/memory.h"

#define DATA_BYTE 1
#include "mov-template.h"
#undef DATA_BYTE

#define DATA_BYTE 2
#include "mov-template.h"
#undef DATA_BYTE

#define DATA_BYTE 4
#include "mov-template.h"
#undef DATA_BYTE

/* for instruction encoding overloading */

make_helper_v(mov_i2r)
make_helper_v(mov_i2rm)
make_helper_v(mov_r2rm)
make_helper_v(mov_rm2r)
make_helper_v(mov_a2moffs)
make_helper_v(mov_moffs2a)

/* Segment register names */
static const char* sregs[] = {"es", "cs", "ss", "ds", "fs", "gs"};

/* Helper function to load segment descriptor cache */
static void load_sreg_cache(int sreg, uint16_t selector) {
	cpu.sreg[sreg].val = selector;
	
	if (!cpu.cr0.protect_enable) {
		/* Real mode: base = selector << 4, limit = 0xFFFF */
		cpu.sreg[sreg].base = selector << 4;
		cpu.sreg[sreg].limit = 0xFFFF;
		return;
	}
	
	/* Protected mode: read from GDT */
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

/* mov rm16, sreg (8C /r) */
make_helper(mov_sreg2rm) {
	ModR_M m;
	m.val = instr_fetch(eip + 1, 1);
	
	int len;
	if (m.mod == 3) {
		/* Register operand */
		reg_w(m.R_M) = cpu.sreg[m.reg].val;
		len = 1;
		print_asm("movw %%%s,%%%s", sregs[m.reg], regsw[m.R_M]);
	} else {
		/* Memory operand */
		len = load_addr(eip + 1, &m, op_dest);
		swaddr_write(op_dest->addr, 2, cpu.sreg[m.reg].val);
		print_asm("movw %%%s,%s", sregs[m.reg], op_dest->str);
	}
	
	return 1 + len;
}

/* mov sreg, rm16 (8E /r) */
make_helper(mov_rm2sreg) {
	ModR_M m;
	m.val = instr_fetch(eip + 1, 1);
	
	int len;
	uint16_t selector;
	
	if (m.mod == 3) {
		/* Register operand */
		selector = reg_w(m.R_M);
		len = 1;
		print_asm("movw %%%s,%%%s", regsw[m.R_M], sregs[m.reg]);
	} else {
		/* Memory operand */
		len = load_addr(eip + 1, &m, op_src);
		selector = swaddr_read(op_src->addr, 2);
		print_asm("movw %s,%%%s", op_src->str, sregs[m.reg]);
	}
	
	/* Load segment register and update descriptor cache */
	load_sreg_cache(m.reg, selector);
	
	return 1 + len;
}
