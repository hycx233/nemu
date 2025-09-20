#include "cpu/exec/template-start.h"

#define instr ret

make_helper(concat(ret_n_, SUFFIX)) {
	DATA_TYPE_S ret_addr = swaddr_read(cpu.esp, DATA_BYTE);
	cpu.esp += DATA_BYTE;
	cpu.eip = ret_addr;
	print_asm("ret");
	return 0;
}

make_helper(concat(ret_i_, SUFFIX)) {
	uint16_t imm = instr_fetch(eip + 1, 2);
	DATA_TYPE_S ret_addr = swaddr_read(cpu.esp, DATA_BYTE);
	cpu.esp += DATA_BYTE + imm;
	cpu.eip = ret_addr;
	print_asm("ret $0x%x", imm);
	return 0;
}

#include "cpu/exec/template-end.h"