#include "cpu/exec/template-start.h"

#define instr test

static void do_execute() {
	DATA_TYPE result = op_dest->val & op_src->val;
	
	// Update flags based on result
	cpu.eflags.ZF = (result == 0);
	cpu.eflags.SF = result >> (DATA_BYTE * 8 - 1);
	cpu.eflags.CF = 0;
	cpu.eflags.OF = 0;
	
	print_asm_template2();
}

make_instr_helper(rm2r)
make_instr_helper(i2a)
make_instr_helper(i2rm)

#include "cpu/exec/template-end.h"