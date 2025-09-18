#include "cpu/exec/template-start.h"

#define instr cmp

static void do_execute() {
	// Perform subtraction: op_dest - op_src, but don't store result
	DATA_TYPE_S result = op_dest->val - op_src->val;
	
	// Update flags based on result
	cpu.eflags.ZF = (result == 0);
	cpu.eflags.SF = result < 0;
	cpu.eflags.CF = op_dest->val < op_src->val;
	cpu.eflags.OF = ((op_dest->val ^ op_src->val) & (op_dest->val ^ result)) >> (DATA_BYTE * 8 - 1);
	
	print_asm_template2();
}

make_instr_helper(i2a)
make_instr_helper(i2rm)  
make_instr_helper(r2rm)
make_instr_helper(rm2r)

#if DATA_BYTE == 2 || DATA_BYTE == 4
make_instr_helper(si2rm)
#endif

#include "cpu/exec/template-end.h"