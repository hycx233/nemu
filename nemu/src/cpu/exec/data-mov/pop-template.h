#include "cpu/exec/template-start.h"

#define instr pop

static void do_execute() {
	// Read data from stack top
	DATA_TYPE data = swaddr_read(cpu.esp, DATA_BYTE);
	
	// Store to destination
	OPERAND_W(op_src, data);
	
	// Adjust stack pointer
	cpu.esp += DATA_BYTE;
	
	print_asm_template1();
}

make_instr_helper(r)

#include "cpu/exec/template-end.h"