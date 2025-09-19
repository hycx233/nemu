#include "cpu/exec/template-start.h"

#define instr lods

make_helper(concat(lods_, SUFFIX)) {
	// Load from [ESI] to accumulator (AL/AX/EAX)
	REG(R_EAX) = MEM_R(cpu.esi);
	
	// Adjust ESI based on DF flag
	cpu.esi += (cpu.eflags.DF ? -DATA_BYTE : DATA_BYTE);

	print_asm("lods" str(SUFFIX) " (%%esi),%%%s", REG_NAME(R_EAX));
	return 1;
}

#include "cpu/exec/template-end.h"