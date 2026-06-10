#pragma once
#include <Windows.h>
#include "Structs.h"
#include "MemoryManager.h"
#include <vector>
#include <string>
#include <map>

struct Label {
	std::string name;
	DWORD address;
	int line;
};

class InstructionManager {
public:
	inline static InstructionManager& getInstructionManager() {
		static InstructionManager instructionManager;
		return instructionManager;
	}

	//StepStatus makeStep();
	StepStatus decodeInstruction(InstructionData& instData);
	StepStatus processInstruction(InstructionData instData);

	StepStatus parseCode(const std::vector<std::string>& lines, std::vector<BYTE>& bytes, std::vector<std::pair<int, StepStatus>>& errors);


private:
	InstructionManager() = default;

	std::vector<Label> getLabels(const std::vector<std::string>& lines);
	void setLabelsAddrs(std::vector<Label>& labels, const std::vector<std::string>& lines);
	bool isLabel(const std::string& line);
	bool isDataLine(const std::string& line);
	std::string parseLabel(const std::vector<Label>& labels, const std::string& labelName);
	StepStatus parseLine(const std::string& line, std::vector<BYTE>& bytes, const std::vector<Label>& labels);
	StepStatus parseDataLine(const std::string& line, std::vector<BYTE>& bytes, const std::vector<Label>& labels);
	std::string parseStringLiteral(const std::string& str);
	BYTE parseOpcode(const std::string opcodeStr);
	std::vector<BYTE> parseOperand(const std::string operandStr, const std::vector<Label>& labels);
	bool isEffAddr(const std::string& operandStr);
	std::vector<BYTE> parseEffAddr(const std::string& operandStr, const std::vector<Label>& labels);
	bool isNumber(const std::string& operandStr);
	std::vector<BYTE> parseNum(const std::string& operandStr);

	const std::map<std::string, Opcode>& getOpcodeDict();
	const std::map<std::string, OperandType>& getOperandDict();

	bool isEmptyLine(const std::string& line);

	StepStatus validateInstruction(const std::vector<BYTE> bytes);
	bool validateEmulatedAddress(OperandData op);
	OperandData decodeOperand(DWORD addr,int rawDataSize, StepStatus* status, int* sizeOut);
	void writeOperand(OperandData op, DWORD data, int size);
	StepStatus writeOperandValue(OperandData op, DWORD data, int size);
	DWORD readOperand(OperandData op, int size);
	DWORD calculateOperandAddressValue(OperandData op, StepStatus& status);
	void* calculateOperandEffAddr(DWORD* opAddr, OperandData op, bool& isEmulatedMemory, StepStatus& status);
	void* getOperandAddr(OperandData op, bool& isEmulatedMemory, StepStatus& status);

	StepStatus decodeNOP(InstructionData& instData);
	StepStatus decodeRET(InstructionData& instData);
	StepStatus decodeFlagInstruction(InstructionData& instData);
	StepStatus decodeSYSCALL(InstructionData& instData);
	StepStatus processSYSCALL(InstructionData instData);
	StepStatus decodeMOV(InstructionData& instData, int opSize);
	void processMOV(InstructionData instData, int opSize);
	StepStatus decodeLEA(InstructionData& instData, int opSize);
	StepStatus processLEA(InstructionData instData, int opSize);
	StepStatus decodeXCHG(InstructionData& instData, int opSize);
	StepStatus processXCHG(InstructionData instData, int opSize);
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
	void processJO(InstructionData instData);
	void processJNO(InstructionData instData);
	void processJS(InstructionData instData);
	void processJNS(InstructionData instData);
	void processJL(InstructionData instData);
	void processJGE(InstructionData instData);
	void processJLE(InstructionData instData);
	void processJG(InstructionData instData);
	StepStatus decodeSUB(InstructionData& instData, int opSize);
	void processSUB(InstructionData instData, int opSize);
	StepStatus decodeADD(InstructionData& instData, int opSize);
	void processADD(InstructionData instData, int opSize);
	StepStatus decodeINC(InstructionData& instData, int opSize);
	StepStatus processINC(InstructionData instData, int opSize);
	StepStatus decodeDEC(InstructionData& instData, int opSize);
	StepStatus processDEC(InstructionData instData, int opSize);
	StepStatus decodeNOT(InstructionData& instData, int opSize);
	StepStatus processNOT(InstructionData instData, int opSize);
	StepStatus decodeNEG(InstructionData& instData, int opSize);
	StepStatus processNEG(InstructionData instData, int opSize);
	StepStatus decodeShift(InstructionData& instData, int opSize);
	StepStatus processSHL(InstructionData instData, int opSize);
	StepStatus processSHR(InstructionData instData, int opSize);
	StepStatus processSAR(InstructionData instData, int opSize);
	StepStatus decodeCALL(InstructionData& instData);
	StepStatus processCALL(InstructionData instData);
	StepStatus processRET(InstructionData instData);
	StepStatus decodeMUL(InstructionData& instData);
	StepStatus processMUL(InstructionData instData);
	StepStatus decodeIMUL(InstructionData& instData);
	StepStatus processIMUL(InstructionData instData);
	StepStatus decodeDIV(InstructionData& instData);
	StepStatus processDIV(InstructionData instData);
	StepStatus decodeIDIV(InstructionData& instData);
	StepStatus processIDIV(InstructionData instData);
	StepStatus decodeTEST(InstructionData& instData, int opSize);
	void processTEST(InstructionData instData, int opSize);
	StepStatus decodeINT3(InstructionData& instData);
	StepStatus decodeJMP(InstructionData& instData);
	void processJMP(InstructionData instData);
	StepStatus decodePUSH(InstructionData& instData);
	StepStatus processPUSH(InstructionData& instData);
	StepStatus decodePOP(InstructionData& instData);
	StepStatus processPOP(InstructionData& instData);

	bool checkParityFlag(DWORD num);


	MemoryManager& mm = MemoryManager::getMemoryManager();
};
