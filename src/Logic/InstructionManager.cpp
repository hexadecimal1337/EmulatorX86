#include "InstructionManager.h"
#include <bitset>

//StepStatus InstructionManager::makeStep() {
//	StepStatus status = EM_OK;
//	InstructionData instData;
//	status = decodeInstruction(instData);
//	if (status >= 0) {
//		processInstruction(instData);
//	}
//	return status;
//}

StepStatus InstructionManager::decodeInstruction(InstructionData& instData) {
	StepStatus status = EM_OK;
	if (mm.ctx.EIP >= 0x8000'0000 || mm.ctx.EIP < 0x0040'0000)
		return EM_INVALID_ADDRESS;
	status = mm.readMem(mm.ctx.EIP, instData.code);
	if (status < 0)
		return status;
	switch (instData.code) {
	case OR:
		status = decodeOR(instData, 4);
		break;
	case AND:
		status = decodeAND(instData, 4);
		break;
	case XOR:
		status = decodeXOR(instData, 4);
		break;
	case CMP:
		status = decodeCMP(instData, 4);
		break;
	case JB: JNB: JE: JNE: JNA: JA:
		status = decodeRelativeJump(instData);
		break;
	case SUB:
		status = decodeSUB(instData, 4);
		break;
	case ADD:
		status = decodeADD(instData, 4);
		break;
	case TEST:
		status = decodeTEST(instData, 4);
		break;
	case MOV:
		status = decodeMOV(instData, 4);
		break;
	case INT3:
		status = decodeINT3(instData);
		break;
	case JMP:
		status = decodeJMP(instData);
		break;
	default:
		status = EM_INVALID_INSTRUCTION;
	}
	if (instData.instAddr + instData.instSize >= 0x8000'0000)
		status = EM_INVALID_ADDRESS;
	return status;
}

void InstructionManager::processInstruction(InstructionData instData) {
	switch (instData.code) {
	case OR:
		processOR(instData, 4);
		break;
	case AND:
		processAND(instData, 4);
		break;
	case XOR:
		processXOR(instData, 4);
		break;
	case CMP:
		processCMP(instData, 4);
		break;
	case JB:
		processJB(instData);
		break;
	case JNB:
		processJNB(instData);
		break;
	case JE:
		processJE(instData);
		break;
	case JNE:
		processJNE(instData);
		break;
	case JNA:
		processJNA(instData);
		break;
	case JA:
		processJA(instData);
		break;
	case SUB:
		processSUB(instData, 4);
		break;
	case ADD:
		processADD(instData, 4);
		break;
	case TEST:
		processTEST(instData, 4);
		break;
	case MOV:
		processMOV(instData, 4);
		break;	
	case JMP:
		processJMP(instData);
		break;
	}
	mm.ctx.EIP += instData.instSize;
}

bool InstructionManager::validateEmulatedAddress(OperandData op) {
	bool isEmulatedMemory = 0;
	StepStatus status = EM_OK;
	unsigned long long addr = (unsigned long long)getOperandAddr(op,isEmulatedMemory, status);
	//if (isEmulatedMemory && (addr + size) >= 0x8000'0000)
	//	status = EM_INVALID_ADDRESS;
	return status >= 0;
}

OperandData InstructionManager::decodeOperand(DWORD addr, int rawDataSize, StepStatus* status, int* sizeOut) {
	*status = EM_INVALID_OPERAND;
	OperandData op;
	*sizeOut = 1;

	*status = mm.readMem(addr, op.mainType);
	op.mainType = (OperandType)(op.mainType & 0x0F);
	if (*status < 0 || op.mainType > OT_RAWDATA || op.mainType == OT_NONE)
		return op;

	*status = mm.readMem(addr,op.effAddr);
	op.effAddr = (EffectiveAddrData)((op.effAddr >> 4) & 0x7);
	if (*status < 0 || (op.mainType == OT_IP && op.effAddr == EAD_DIRECT))
		return op;

	if (op.mainType == OT_RAWDATA && op.effAddr > 1) {
		*status = EM_INVALID_OPERAND;
		return op;
	}
		
	op.OpAddr = addr;

	if (op.mainType == OT_RAWDATA) {
		switch (rawDataSize) {
		case 1:
			*status = mm.readMem(addr + 1, (BYTE&)op.rawData);
			*sizeOut += 1;
			break;
		case 2:
			*status = mm.readMem(addr + 1, (WORD&)op.rawData);
			*sizeOut += 2;
			break;
		case 4:
			*status = mm.readMem(addr + 1, (DWORD&)op.rawData);
			*sizeOut += 4;
			break;
		
		default:
			*status = EM_INVALID_OPERAND;
			return op;
		}
	}
	else if (op.effAddr > EAD_READ) {
		if (op.effAddr <= EAD_MUL8_READ) {
			*status = mm.readMem(addr + 1, (BYTE&)op.rawData);
			*sizeOut += 1;
		}
		else if (op.effAddr <= EAD_MUL32_READ) {
			*status = mm.readMem(addr + 1, (DWORD&)op.rawData);
			*sizeOut += 4;
		}
		else {
			*status = mm.readMem(addr + 1, op.effAddrRegType);
			bool effAddrFlag = op.effAddrRegType >> 4;
			op.effAddrRegType = (OperandType)(op.effAddrRegType & 0x0F);
			if (op.effAddrRegType >= OT_RAWDATA || op.effAddrRegType == OT_NONE || effAddrFlag) {
				*status = EM_INVALID_OPERAND;
				return op;
			}
			*sizeOut += 1;
		}
	}

	if (!validateEmulatedAddress(op))
		*status = EM_INVALID_ADDRESS;
	else
		*status = EM_OK;
	return op;
}

void InstructionManager::writeOperand(OperandData op, DWORD data, int size) {
	bool isEmulatedAddress = 0;
	StepStatus status = EM_OK;
	void* addr = getOperandAddr(op, isEmulatedAddress, status);
	if (!isEmulatedAddress) {
		if (size == 1)
			*((BYTE*)addr + 3) = data;
		else if (size == 2)
			*((WORD*)addr + 2) = data;
		else if (size == 4)
			*(DWORD*)addr = data;
	}
}

DWORD InstructionManager::readOperand(OperandData op, int size) {
	DWORD result = 0;
	bool isEmulatedAddress = 0;
	StepStatus status = EM_OK;
	void* addr = getOperandAddr(op,isEmulatedAddress,status);
	if (!isEmulatedAddress) {
		if (size == 1)
			result = *((BYTE*)addr + 3);
		else if (size == 2)
			result = *((WORD*)addr + 2);
		else if (size == 4)
			result = *(DWORD*)addr;
	}
	return result;
}

void* InstructionManager::calculateOperandEffAddr(DWORD* opAddr, OperandData op, bool& isEmulatedMemory, StepStatus& status) {
	void* result = nullptr;
	if (op.effAddr == EAD_DIRECT)
		result = opAddr;
	else if (op.effAddr == EAD_READ) {
		if (isEmulatedMemory)
			status = mm.readMem((DWORD)opAddr, (DWORD&)result);
		else
			result = (void*)*opAddr;
		isEmulatedMemory = true;
	}
	else if (op.effAddr == EAD_ADD8_READ || op.effAddr == EAD_ADD32_READ) {
		if (isEmulatedMemory)
			status = mm.readMem((DWORD)opAddr + op.rawData, (DWORD&)result);
		else
			result = (void*)(*opAddr + op.rawData);
		isEmulatedMemory = true;
	}
	else if (op.effAddr == EAD_MUL8_READ || op.effAddr == EAD_MUL32_READ) {
		if (isEmulatedMemory)
			status = mm.readMem((DWORD)opAddr * op.rawData, (DWORD&)result);
		else
			result = (void*)(*opAddr * op.rawData);
		isEmulatedMemory = true;
	}
	else if (op.effAddr == EAD_ADDREG_READ) {
		OperandData op2;
		op2.mainType = op.effAddrRegType;
		if (isEmulatedMemory)
			status = mm.readMem((DWORD)opAddr + readOperand(op2, 4), (DWORD&)result);
		else
			result = (void*)(*opAddr + readOperand(op2, 4));
		isEmulatedMemory = true;
	}
	else if (op.effAddr == EAD_MULREG_READ) {
		OperandData op2;
		op2.mainType = op.effAddrRegType;
		if (isEmulatedMemory)
			status = mm.readMem((DWORD)opAddr * readOperand(op2, 4), (DWORD&)result);
		else
			result = (void*)(*opAddr * readOperand(op2, 4));
		isEmulatedMemory = true;
	}
	if (isEmulatedMemory && (unsigned long long)result >= 0x8000'0000ull)
		status = EM_INVALID_ADDRESS;
	return result;
}

void* InstructionManager::getOperandAddr(OperandData op, bool& isEmulatedMemory, StepStatus& status) {
	status = EM_OK;
	DWORD* opAddr = nullptr;
	isEmulatedMemory = false;
	switch (op.mainType) {
	case OT_A:
		opAddr = &mm.ctx.EAX;
		break;
	case OT_B:
		opAddr = &mm.ctx.EBX;
		break;
	case OT_C:
		opAddr = &mm.ctx.ECX;
		break;
	case OT_D:
		opAddr = &mm.ctx.EDX;
		break;
	case OT_SP:
		opAddr = &mm.ctx.ESP;
		break;
	case OT_BP:
		opAddr = &mm.ctx.EBP;
		break;
	case OT_SI:
		opAddr = &mm.ctx.ESI;
		break;
	case OT_DI:
		opAddr = &mm.ctx.EDI;
		break;
	case OT_IP:
		opAddr = &mm.ctx.EIP;
		break;
	case OT_RAWDATA:
		opAddr = (DWORD*)(op.OpAddr + 1);
		isEmulatedMemory = true;
		break;
	}
	return calculateOperandEffAddr(opAddr,op, isEmulatedMemory, status);
}

StepStatus InstructionManager::decodeMOV(InstructionData& instData, int opSize) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, opSize,&status,&lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	instData.op2 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	if (instData.op1.mainType == OT_RAWDATA && instData.op1.effAddr != EAD_READ)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.effAddr > 0)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.mainType == OT_RAWDATA && instData.op2.effAddr == EAD_DIRECT)
		return EM_INVALID_OPERAND;

	instData.instSize = counter;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 2;
	

	return status;
}

void InstructionManager::processMOV(InstructionData instData, int opSize) {
	bool isDestEmulated = false, isSrcEmulated = false;
	StepStatus status = EM_OK;
	DWORD srcData = 0;
	void* src = getOperandAddr(instData.op2, isSrcEmulated, status);
	void* dest = getOperandAddr(instData.op1,isDestEmulated,status);
	if (isSrcEmulated)
		mm.readMem((DWORD)src, srcData);
	else
		srcData = *(DWORD*)src;
	if (isDestEmulated)
		mm.writeMem((DWORD)dest, srcData);
	else
		*(DWORD*)dest = srcData;
}

StepStatus InstructionManager::decodeOR(InstructionData& instData, int opSize) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	instData.op2 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	if (instData.op1.mainType == OT_RAWDATA && instData.op1.effAddr != EAD_READ)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.effAddr > 0)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.mainType == OT_RAWDATA && instData.op2.effAddr == EAD_DIRECT)
		return EM_INVALID_OPERAND;

	instData.instSize = counter;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 1;

	return status;
}

void InstructionManager::processOR(InstructionData instData, int andSize) {
	bool isDestEmulated = false, isSrcEmulated = false;
	StepStatus status = EM_OK;
	DWORD srcData = 0, destData = 0;
	void* src = getOperandAddr(instData.op2, isSrcEmulated, status);
	void* dest = getOperandAddr(instData.op1, isDestEmulated, status);
	if (isSrcEmulated)
		mm.readMem((DWORD)src, srcData);
	else
		srcData = *(DWORD*)src;
	if (isDestEmulated) {
		mm.readMem((DWORD)dest, destData);
		destData |= srcData;
		mm.writeMem((DWORD)dest, destData);
	}
	else {
		destData = *(DWORD*)dest;
		destData |= srcData;
		*(DWORD*)dest = destData;
	}
	mm.ctx.EFLAGS[EFLAG_CF] = 0;
	mm.ctx.EFLAGS[EFLAG_OF] = 0;
	mm.ctx.EFLAGS[EFLAG_SF] = destData & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !destData;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(destData);
}

StepStatus InstructionManager::decodeAND(InstructionData& instData, int opSize) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	instData.op2 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	if (instData.op1.mainType == OT_RAWDATA && instData.op1.effAddr != EAD_READ)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.effAddr > 0)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.mainType == OT_RAWDATA && instData.op2.effAddr == EAD_DIRECT)
		return EM_INVALID_OPERAND;

	instData.instSize = counter;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 1;

	return status;
}

void InstructionManager::processAND(InstructionData instData, int opSize) {
	bool isDestEmulated = false, isSrcEmulated = false;
	StepStatus status = EM_OK;
	DWORD srcData = 0, destData = 0;
	void* src = getOperandAddr(instData.op2, isSrcEmulated, status);
	void* dest = getOperandAddr(instData.op1, isDestEmulated, status);
	if (isSrcEmulated)
		mm.readMem((DWORD)src, srcData);
	else
		srcData = *(DWORD*)src;
	if (isDestEmulated) {
		mm.readMem((DWORD)dest, destData);
		destData &= srcData;
		mm.writeMem((DWORD)dest, destData);
	}
	else {
		destData = *(DWORD*)dest;
		destData &= srcData;
		*(DWORD*)dest = destData;
	}
	mm.ctx.EFLAGS[EFLAG_CF] = 0;
	mm.ctx.EFLAGS[EFLAG_OF] = 0;
	mm.ctx.EFLAGS[EFLAG_SF] = destData & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !destData;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(destData);
}

StepStatus InstructionManager::decodeXOR(InstructionData& instData, int opSize) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	instData.op2 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	if (instData.op1.mainType == OT_RAWDATA && instData.op1.effAddr != EAD_READ)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.effAddr > 0)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.mainType == OT_RAWDATA && instData.op2.effAddr == EAD_DIRECT)
		return EM_INVALID_OPERAND;

	instData.instSize = counter;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 1;

	return status;
}

void InstructionManager::processXOR(InstructionData instData, int andSize) {
	bool isDestEmulated = false, isSrcEmulated = false;
	StepStatus status = EM_OK;
	DWORD srcData = 0, destData = 0;
	void* src = getOperandAddr(instData.op2, isSrcEmulated, status);
	void* dest = getOperandAddr(instData.op1, isDestEmulated, status);
	if (isSrcEmulated)
		mm.readMem((DWORD)src, srcData);
	else
		srcData = *(DWORD*)src;
	if (isDestEmulated) {
		mm.readMem((DWORD)dest, destData);
		destData ^= srcData;
		mm.writeMem((DWORD)dest, destData);
	}
	else {
		destData = *(DWORD*)dest;
		destData ^= srcData;
		*(DWORD*)dest = destData;
	}
	mm.ctx.EFLAGS[EFLAG_CF] = 0;
	mm.ctx.EFLAGS[EFLAG_OF] = 0;
	mm.ctx.EFLAGS[EFLAG_SF] = destData & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !destData;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(destData);
}

StepStatus InstructionManager::decodeCMP(InstructionData& instData, int opSize) {
	return decodeSUB(instData, opSize);
}

void InstructionManager::processCMP(InstructionData instData, int andSize) {
	bool isDestEmulated = false, isSrcEmulated = false;
	StepStatus status = EM_OK;
	DWORD srcData = 0, destData = 0, result = 0;
	void* src = getOperandAddr(instData.op2, isSrcEmulated, status);
	void* dest = getOperandAddr(instData.op1, isDestEmulated, status);
	if (isSrcEmulated)
		mm.readMem((DWORD)src, srcData);
	else
		srcData = *(DWORD*)src;
	if (isDestEmulated) {
		mm.readMem((DWORD)dest, destData);
		result = destData - srcData;
	}
	else {
		destData = *(DWORD*)dest;
		result = destData - srcData;
	}
	mm.ctx.EFLAGS[EFLAG_CF] = result < srcData;
	mm.ctx.EFLAGS[EFLAG_OF] = 0; // need add a check
	mm.ctx.EFLAGS[EFLAG_SF] = result & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !result;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(result);
}

StepStatus InstructionManager::decodeRelativeJump(InstructionData& instData) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, 2, &status, &lastOperandSize);
	if (status < 0)
		return status;
	if (instData.op1.effAddr != EAD_DIRECT || instData.op1.mainType != OT_RAWDATA)
		return EM_INVALID_OPERAND;
	counter += lastOperandSize;
	instData.instSize = counter;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 2;
	return status;
}

void InstructionManager::processJB(InstructionData instData) {
	if (mm.ctx.EFLAGS[EFLAG_CF])
		mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
}

void InstructionManager::processJNB(InstructionData instData) {
	if (!mm.ctx.EFLAGS[EFLAG_CF])
		mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
}

void InstructionManager::processJE(InstructionData instData) {
	if (mm.ctx.EFLAGS[EFLAG_ZF])
		mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
}

void InstructionManager::processJNE(InstructionData instData) {
	if (!mm.ctx.EFLAGS[EFLAG_ZF])
		mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
}

void InstructionManager::processJNA(InstructionData instData) {
	if (mm.ctx.EFLAGS[EFLAG_CF] && mm.ctx.EFLAGS[EFLAG_ZF])
		mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
}

void InstructionManager::processJA(InstructionData instData) {
	if (!mm.ctx.EFLAGS[EFLAG_CF] && !mm.ctx.EFLAGS[EFLAG_ZF])
		mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
}

StepStatus InstructionManager::decodeSUB(InstructionData& instData, int opSize) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	instData.op2 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	if (instData.op1.mainType == OT_RAWDATA && instData.op1.effAddr != EAD_READ)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.effAddr > 0)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.mainType == OT_RAWDATA && instData.op2.effAddr == EAD_DIRECT)
		return EM_INVALID_OPERAND;

	instData.instSize = counter;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 2;

	return status;
}

void InstructionManager::processSUB(InstructionData instData, int andSize) {
	bool isDestEmulated = false, isSrcEmulated = false;
	StepStatus status = EM_OK;
	DWORD srcData = 0, destData = 0, result = 0;
	void* src = getOperandAddr(instData.op2, isSrcEmulated, status);
	void* dest = getOperandAddr(instData.op1, isDestEmulated, status);
	if (isSrcEmulated)
		mm.readMem((DWORD)src, srcData);
	else
		srcData = *(DWORD*)src;
	if (isDestEmulated) {
		mm.readMem((DWORD)dest, destData);
		result = destData - srcData;
		mm.writeMem((DWORD)dest, result);
	}
	else {
		destData = *(DWORD*)dest;
		result = destData - srcData;
		*(DWORD*)dest = result;
	}
	mm.ctx.EFLAGS[EFLAG_CF] = destData > srcData; 
	mm.ctx.EFLAGS[EFLAG_OF] = 0; // need add a check
	mm.ctx.EFLAGS[EFLAG_SF] = result & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !result;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(result);
}

StepStatus InstructionManager::decodeADD(InstructionData& instData, int opSize) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	instData.op2 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	if (instData.op1.mainType == OT_RAWDATA && instData.op1.effAddr != EAD_READ)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.effAddr > 0)
		return EM_INVALID_OPERAND;
	if (instData.op1.effAddr && instData.op2.mainType == OT_RAWDATA && instData.op2.effAddr == EAD_DIRECT)
		return EM_INVALID_OPERAND;

	instData.instSize = counter;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 2;

	return status;
}

void InstructionManager::processADD(InstructionData instData, int andSize) {
	bool isDestEmulated = false, isSrcEmulated = false;
	StepStatus status = EM_OK;
	DWORD srcData = 0, destData = 0, result = 0;
	void* src = getOperandAddr(instData.op2, isSrcEmulated, status);
	void* dest = getOperandAddr(instData.op1, isDestEmulated, status);
	if (isSrcEmulated)
		mm.readMem((DWORD)src, srcData);
	else
		srcData = *(DWORD*)src;
	if (isDestEmulated) {
		mm.readMem((DWORD)dest, destData);
		destData = (int)srcData + (int)destData;
		mm.writeMem((DWORD)dest, result);
	}
	else {
		destData = *(DWORD*)dest;
		result = srcData + destData;
		*(DWORD*)dest = result;
	}
	mm.ctx.EFLAGS[EFLAG_CF] = 0; // need add a support x8/x16 operations
	mm.ctx.EFLAGS[EFLAG_OF] = 0; // need add a check
	mm.ctx.EFLAGS[EFLAG_SF] = result & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !result;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(result);
}

StepStatus InstructionManager::decodeTEST(InstructionData& instData, int opSize) {
	return decodeAND(instData, opSize);
}

void InstructionManager::processTEST(InstructionData instData, int andSize) {
	bool isDestEmulated = false, isSrcEmulated = false;
	StepStatus status = EM_OK;
	DWORD srcData = 0, destData = 0;
	void* src = getOperandAddr(instData.op2, isSrcEmulated, status);
	void* dest = getOperandAddr(instData.op1, isDestEmulated, status);
	if (isSrcEmulated)
		mm.readMem((DWORD)src, srcData);
	else
		srcData = *(DWORD*)src;
	if (isDestEmulated) {
		mm.readMem((DWORD)dest, destData);
		destData &= srcData;
	}
	else {
		destData = *(DWORD*)dest;
		destData &= srcData;
	}
	mm.ctx.EFLAGS[EFLAG_CF] = 0;
	mm.ctx.EFLAGS[EFLAG_OF] = 0;
	mm.ctx.EFLAGS[EFLAG_SF] = destData & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !destData;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(destData);
}

StepStatus InstructionManager::decodeINT3(InstructionData& instData) {
	StepStatus status = EM_BREAKPOINT;
	instData.instSize = 1;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 10;
	return status;
}

StepStatus InstructionManager::decodeJMP(InstructionData& instData) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, 4, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	instData.instSize = 0; // disable EIP addiction after processing
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 5;
	return status;
}

void InstructionManager::processJMP(InstructionData instData) {
	bool isAddrEmulated = false;
	StepStatus status = EM_OK;
	DWORD addr = 0;
	void* addrOfAddr = getOperandAddr(instData.op1, isAddrEmulated, status);
	if (isAddrEmulated)
		mm.readMem((DWORD)addrOfAddr, mm.ctx.EIP);
	else
		mm.ctx.EIP = *(DWORD*)addrOfAddr;
}

bool InstructionManager::checkParityFlag(DWORD num) {
	std::bitset<32> bits(num);
	return !(bits.count() % 2);
}

