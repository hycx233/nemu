#include "cpu/exec/helper.h"

make_helper(setcc_rm_b) {
	bool condition_met = false;
	const char* mnemonic = "set??";
	
	uint8_t opcode = ops_decoded.opcode & 0xFF;
	
	switch(opcode) {
		case 0x90: // SETO - Set if Overflow
			condition_met = cpu.eflags.OF;
			mnemonic = "seto";
			break;
		case 0x91: // SETNO - Set if Not Overflow
			condition_met = !cpu.eflags.OF;
			mnemonic = "setno";
			break;
		case 0x92: // SETB/SETC/SETNAE - Set if Below/Carry/Not Above or Equal
			condition_met = cpu.eflags.CF;
			mnemonic = "setb";
			break;
		case 0x93: // SETNB/SETNC/SETAE - Set if Not Below/Not Carry/Above or Equal
			condition_met = !cpu.eflags.CF;
			mnemonic = "setnb";
			break;
		case 0x94: // SETE/SETZ - Set if Equal/Zero
			condition_met = cpu.eflags.ZF;
			mnemonic = "sete";
			break;
		case 0x95: // SETNE/SETNZ - Set if Not Equal/Not Zero
			condition_met = !cpu.eflags.ZF;
			mnemonic = "setne";
			break;
		case 0x96: // SETBE/SETNA - Set if Below or Equal/Not Above
			condition_met = cpu.eflags.CF || cpu.eflags.ZF;
			mnemonic = "setbe";
			break;
		case 0x97: // SETNBE/SETA - Set if Not Below or Equal/Above
			condition_met = !cpu.eflags.CF && !cpu.eflags.ZF;
			mnemonic = "seta";
			break;
		case 0x98: // SETS - Set if Sign
			condition_met = cpu.eflags.SF;
			mnemonic = "sets";
			break;
		case 0x99: // SETNS - Set if Not Sign
			condition_met = !cpu.eflags.SF;
			mnemonic = "setns";
			break;
		case 0x9A: // SETP/SETPE - Set if Parity/Parity Even
			condition_met = cpu.eflags.PF;
			mnemonic = "setp";
			break;
		case 0x9B: // SETNP/SETPO - Set if Not Parity/Parity Odd
			condition_met = !cpu.eflags.PF;
			mnemonic = "setnp";
			break;
		case 0x9C: // SETL/SETNGE - Set if Less/Not Greater or Equal
			condition_met = (cpu.eflags.SF != cpu.eflags.OF);
			mnemonic = "setl";
			break;
		case 0x9D: // SETNL/SETGE - Set if Not Less/Greater or Equal
			condition_met = (cpu.eflags.SF == cpu.eflags.OF);
			mnemonic = "setnl";
			break;
		case 0x9E: // SETLE/SETNG - Set if Less or Equal/Not Greater
			condition_met = cpu.eflags.ZF || (cpu.eflags.SF != cpu.eflags.OF);
			mnemonic = "setle";
			break;
		case 0x9F: // SETNLE/SETG - Set if Not Less or Equal/Greater
			condition_met = !cpu.eflags.ZF && (cpu.eflags.SF == cpu.eflags.OF);
			mnemonic = "setg";
			break;
		default:
			panic("Unknown setcc opcode: 0x%02x", opcode);
			break;
	}
	
	// Decode r/m8 operand
	int len = decode_rm_b(eip + 1);
	
	// Set destination to 1 if condition is met, 0 otherwise
	uint8_t result = condition_met ? 1 : 0;
	write_operand_b(op_src, result);
	
	print_asm("%s %s", mnemonic, op_src->str);
	return len + 1;
}