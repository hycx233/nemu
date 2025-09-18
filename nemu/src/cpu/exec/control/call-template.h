#include "cpu/exec/template-start.h"

#define instr call

static void do_execute() {
	// calc return address
	swaddr_t return_addr = cpu.eip + 1 + DATA_BYTE;
	
	// push return address onto stack
	cpu.esp -= 4;
	swaddr_write(cpu.esp, 4, return_addr);
	
	// jump to target address
	cpu.eip += op_src->val;
	
	print_asm("call %x", cpu.eip + 1 + DATA_BYTE);
}

make_instr_helper(si)

#include "cpu/exec/template-end.h"
