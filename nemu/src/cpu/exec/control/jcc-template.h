#include "cpu/exec/template-start.h"

#define instr jcc

static void do_execute() {
	bool should_jump = false;
	const char* mnemonic = "j??";
	
	uint8_t opcode = ops_decoded.opcode & 0xFF;
	
	switch(opcode) {
		case 0x70: // JO - Jump if Overflow
			should_jump = cpu.eflags.OF;
			mnemonic = "jo";
			break;
		case 0x71: // JNO - Jump if Not Overflow
			should_jump = !cpu.eflags.OF;
			mnemonic = "jno";
			break;
		case 0x72: // JB/JC/JNAE - Jump if Below/Carry/Not Above or Equal
			should_jump = cpu.eflags.CF;
			mnemonic = "jb";
			break;
		case 0x73: // JNB/JNC/JAE - Jump if Not Below/Not Carry/Above or Equal
			should_jump = !cpu.eflags.CF;
			mnemonic = "jnb";
			break;
		case 0x74: // JE/JZ - Jump if Equal/Zero
			should_jump = cpu.eflags.ZF;
			mnemonic = "je";
			break;
		case 0x75: // JNE/JNZ - Jump if Not Equal/Not Zero
			should_jump = !cpu.eflags.ZF;
			mnemonic = "jne";
			break;
		case 0x76: // JBE/JNA - Jump if Below or Equal/Not Above
			should_jump = cpu.eflags.CF || cpu.eflags.ZF;
			mnemonic = "jbe";
			break;
		case 0x77: // JNBE/JA - Jump if Not Below or Equal/Above
			should_jump = !cpu.eflags.CF && !cpu.eflags.ZF;
			mnemonic = "ja";
			break;
		case 0x78: // JS - Jump if Sign
			should_jump = cpu.eflags.SF;
			mnemonic = "js";
			break;
		case 0x79: // JNS - Jump if Not Sign
			should_jump = !cpu.eflags.SF;
			mnemonic = "jns";
			break;
		case 0x7A: // JP/JPE - Jump if Parity/Parity Even
			should_jump = cpu.eflags.PF;
			mnemonic = "jp";
			break;
		case 0x7B: // JNP/JPO - Jump if Not Parity/Parity Odd
			should_jump = !cpu.eflags.PF;
			mnemonic = "jnp";
			break;
		case 0x7C: // JL/JNGE - Jump if Less/Not Greater or Equal
			should_jump = (cpu.eflags.SF != cpu.eflags.OF);
			mnemonic = "jl";
			break;
		case 0x7D: // JNL/JGE - Jump if Not Less/Greater or Equal
			should_jump = (cpu.eflags.SF == cpu.eflags.OF);
			mnemonic = "jnl";
			break;
		case 0x7E: // JLE/JNG - Jump if Less or Equal/Not Greater
			should_jump = cpu.eflags.ZF || (cpu.eflags.SF != cpu.eflags.OF);
			mnemonic = "jle";
			break;
		case 0x7F: // JNLE/JG - Jump if Not Less or Equal/Greater
			should_jump = !cpu.eflags.ZF && (cpu.eflags.SF == cpu.eflags.OF);
			mnemonic = "jg";
			break;
		// For 0F xx opcodes (two-byte instructions)
		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8A: case 0x8B:
		case 0x8C: case 0x8D: case 0x8E: case 0x8F:
			// Handle two-byte conditional jumps (0F 8x)
			switch(opcode) {
				case 0x80: should_jump = cpu.eflags.OF; mnemonic = "jo"; break;
				case 0x81: should_jump = !cpu.eflags.OF; mnemonic = "jno"; break;
				case 0x82: should_jump = cpu.eflags.CF; mnemonic = "jb"; break;
				case 0x83: should_jump = !cpu.eflags.CF; mnemonic = "jnb"; break;
				case 0x84: should_jump = cpu.eflags.ZF; mnemonic = "je"; break;
				case 0x85: should_jump = !cpu.eflags.ZF; mnemonic = "jne"; break;
				case 0x86: should_jump = cpu.eflags.CF || cpu.eflags.ZF; mnemonic = "jbe"; break;
				case 0x87: should_jump = !cpu.eflags.CF && !cpu.eflags.ZF; mnemonic = "ja"; break;
				case 0x88: should_jump = cpu.eflags.SF; mnemonic = "js"; break;
				case 0x89: should_jump = !cpu.eflags.SF; mnemonic = "jns"; break;
				case 0x8A: should_jump = cpu.eflags.PF; mnemonic = "jp"; break;
				case 0x8B: should_jump = !cpu.eflags.PF; mnemonic = "jnp"; break;
				case 0x8C: should_jump = (cpu.eflags.SF != cpu.eflags.OF); mnemonic = "jl"; break;
				case 0x8D: should_jump = (cpu.eflags.SF == cpu.eflags.OF); mnemonic = "jnl"; break;
				case 0x8E: should_jump = cpu.eflags.ZF || (cpu.eflags.SF != cpu.eflags.OF); mnemonic = "jle"; break;
				case 0x8F: should_jump = !cpu.eflags.ZF && (cpu.eflags.SF == cpu.eflags.OF); mnemonic = "jg"; break;
			}
			break;
		default:
			panic("Unknown jcc opcode: 0x%02x", opcode);
			break;
	}
	
	if(should_jump) {
		DATA_TYPE_S offset = op_src->val;
		cpu.eip += offset;
	}
	
	print_asm("%s %x", mnemonic, cpu.eip + 1 + DATA_BYTE + op_src->val);
}

make_instr_helper(si)

#include "cpu/exec/template-end.h"