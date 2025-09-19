#include "cpu/exec/helper.h"

make_helper(leave) {
	// LEAVE instruction: MOV ESP, EBP; POP EBP
	
	// MOV ESP, EBP
	cpu.esp = cpu.ebp;
	
	// POP EBP (read from stack and increment ESP)
	cpu.ebp = swaddr_read(cpu.esp, 4);
	cpu.esp += 4;
	
	print_asm("leave");
	return 1;
}