#pragma once
#include "Structs.h"
#include "MemoryManager.h"

class InstructionManager {
public:
	inline static InstructionManager& getInstructionManager() {
		static InstructionManager instructionManager;
		return instructionManager;
	}

	//StepStatus makeStep();
	StepStatus decodeInstruction(InstructionData& instData);
	void processInstruction(InstructionData instData);


private:
	InstructionManager() = default;
	
	bool validateEmulatedAddress(OperandData op);
	OperandData decodeOperand(DWORD addr,int rawDataSize, StepStatus* status, int* sizeOut);
	void writeOperand(OperandData op, DWORD data, int size);
	DWORD readOperand(OperandData op, int size);
	void* calculateOperandEffAddr(DWORD* opAddr, OperandData op, bool& isEmulatedMemory, StepStatus& status);
	void* getOperandAddr(OperandData op, bool& isEmulatedMemory, StepStatus& status);

	StepStatus decodeMOV(InstructionData& instData, int opSize);
	void processMOV(InstructionData instData, int opSize);
	StepStatus decodeOR(InstructionData& instData, int opSize);
	void processOR(InstructionData instData, int opSize);
	StepStatus decodeAND(InstructionData& instData, int opSize);
	void processAND(InstructionData instData, int opSize);
	StepStatus decodeXOR(InstructionData& instData, int opSize);
	void processXOR(InstructionData instData, int opSize);
	StepStatus decodeCMP(InstructionData& instData, int opSize);
	void processCMP(InstructionData instData, int opSize);
	StepStatus decodeRelativeJump(InstructionData& instData);
	void processJB(InstructionData instData);
	void processJNB(InstructionData instData);
	void processJE(InstructionData instData);
	void processJNE(InstructionData instData);
	void processJNA(InstructionData instData);
	void processJA(InstructionData instData);
	StepStatus decodeSUB(InstructionData& instData, int opSize);
	void processSUB(InstructionData instData, int opSize);
	StepStatus decodeADD(InstructionData& instData, int opSize);
	void processADD(InstructionData instData, int opSize);
	StepStatus decodeTEST(InstructionData& instData, int opSize);
	void processTEST(InstructionData instData, int opSize);
	StepStatus decodeINT3(InstructionData& instData);
	StepStatus decodeJMP(InstructionData& instData);
	void processJMP(InstructionData instData);


	bool checkParityFlag(DWORD num);

	MemoryManager& mm = MemoryManager::getMemoryManager();
};