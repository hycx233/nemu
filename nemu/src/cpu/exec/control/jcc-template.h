#include "cpu/exec/template-start.h"

#define instr jcc

static void do_execute() {
	if(ops_decoded.opcode == 0x74 || ops_decoded.opcode == 0x84) {
		// je: jump if ZF = 1
		if(cpu.eflags.ZF) {
			cpu.eip += op_src->val;
		}
	}
	print_asm("je %x", cpu.eip + 1 + DATA_BYTE);
}

make_instr_helper(si)

#include "cpu/exec/template-end.h"