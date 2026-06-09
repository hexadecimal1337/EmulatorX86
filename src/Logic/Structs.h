#pragma once
#include <windows.h>

enum StepStatus {
	EM_OK,
	EM_BREAKPOINT = 1,
	EM_INVALID_INSTRUCTION = -1,
	EM_INVALID_OPERAND = -2,
	EM_INVALID_ADDRESS = -3,
	EM_INVALID_SYNTAX = -4
};

enum OperandType : BYTE {
	OT_NONE,
	OT_A,
	OT_B,
	OT_C,
	OT_D,
	OT_SP,
	OT_BP,
	OT_SI,
	OT_DI,
	OT_IP,
	OT_RAWDATA
};

enum EffectiveAddrData : BYTE {
	EAD_DIRECT,
	EAD_READ,
	EAD_ADD8_READ,
	EAD_MUL8_READ,
	EAD_ADD32_READ,
	EAD_MUL32_READ,
	EAD_ADDREG_READ,
	EAD_MULREG_READ
};

struct OperandData {
	OperandType mainType = OT_NONE;
	EffectiveAddrData effAddr = EAD_DIRECT;
	OperandType effAddrRegType = OT_NONE;
	DWORD rawData = 0;
	DWORD OpAddr = 0;
};

struct InstructionData {
	BYTE code = 0;
	OperandData op1, op2, op3;
	DWORD instSize = 0;
	DWORD instAddr = 0;
	DWORD ticks = 0;
};

enum ByteDirection {
	BDir_Little_Endian,
	BDir_Big_Endian
};

enum Opcode : BYTE {
	OR = 0x9,
	AND = 0x21,
	XOR = 0x31,
	CMP = 0x39,
	PUSH = 0x50,
	POP = 0x58,
	JB = 0x72,
	JNB = 0x73,
	JE = 0x74,
	JNE = 0x75,
	JBE = 0x76,
	JA = 0x77,
	SUB = 0x81,
	ADD = 0x83,
	TEST = 0x85,
	MOV = 0x8B,
	INT3 = 0xCC,
	JMP = 0xE9
};

struct ThreadContext {
	DWORD EAX;
	DWORD EBX;
	DWORD ECX;
	DWORD EDX;
	DWORD EBP;
	DWORD ESP;
	DWORD ESI;
	DWORD EDI;
	DWORD EIP;

	bool EFLAGS[32];

	int frequency;
	unsigned int counter;
};

enum ThreadFlag {
	EFLAG_CF = 0,
	EFLAG_PF = 2,
	EFLAG_AF = 4,
	EFLAG_ZF = 6,
	EFLAG_SF = 7,
	EFLAG_OF = 11
};