#include "InstructionManager.h"
#include "ProgramConsoleManager.h"
#include <algorithm>
#include <bitset>
#include <cctype>
#include <limits>
#include <regex>
#include <sstream>

namespace {
constexpr DWORD MAX_STRING_READ = 1024 * 1024;

StepStatus readUtf8String(MemoryManager& mm, DWORD addr, std::string& text, DWORD maxBytes = MAX_STRING_READ) {
	text.clear();
	for (DWORD i = 0; i < maxBytes; ++i) {
		BYTE byte = 0;
		StepStatus status = mm.readMem(addr + i, byte);
		if (status < 0)
			return status;
		if (byte == 0)
			break;
		text.push_back((char)byte);
	}
	return EM_OK;
}

StepStatus writeUtf8String(MemoryManager& mm, DWORD addr, const std::string& text) {
	for (DWORD i = 0; i < text.size(); ++i) {
		StepStatus status = mm.writeMem(addr + i, (BYTE)text[i]);
		if (status < 0)
			return status;
	}
	return mm.writeMem(addr + (DWORD)text.size(), (BYTE)0);
}

std::string trim(std::string text) {
	size_t begin = 0;
	while (begin < text.size() && std::isspace((unsigned char)text[begin]))
		++begin;
	size_t end = text.size();
	while (end > begin && std::isspace((unsigned char)text[end - 1]))
		--end;
	return text.substr(begin, end - begin);
}

std::string toLower(std::string text) {
	for (char& sym : text)
		sym = (char)std::tolower((unsigned char)sym);
	return text;
}

bool isFormatSpecifier(char specifier) {
	switch (specifier) {
	case 's':
	case 'd':
	case 'u':
	case 'x':
	case 'X':
	case 'c':
		return true;
	default:
		return false;
	}
}

StepStatus readStackArgument(MemoryManager& mm, DWORD esp, DWORD argIndex, DWORD& arg) {
	return mm.readMem(esp + 4 + argIndex * 4, arg);
}

StepStatus formatStackArguments(MemoryManager& mm, const std::string& format, DWORD esp, std::string& output) {
	output.clear();
	DWORD argIndex = 0;
	for (size_t i = 0; i < format.size(); ++i) {
		if (format[i] != '%' || i + 1 >= format.size()) {
			output.push_back(format[i]);
			continue;
		}
		char specifier = format[++i];
		if (specifier == '%') {
			output.push_back('%');
			continue;
		}
		if (!isFormatSpecifier(specifier)) {
			output.push_back('%');
			output.push_back(specifier);
			continue;
		}
		DWORD arg = 0;
		StepStatus status = readStackArgument(mm, esp, argIndex++, arg);
		if (status < 0)
			return status;
		std::ostringstream stream;
		switch (specifier) {
		case 's': {
			std::string text;
			status = readUtf8String(mm, arg, text);
			if (status < 0)
				return status;
			output += text;
			break;
		}
		case 'd':
			stream << (int)arg;
			output += stream.str();
			break;
		case 'u':
			stream << arg;
			output += stream.str();
			break;
		case 'x':
		case 'X':
			stream << std::hex;
			if (specifier == 'X')
				stream << std::uppercase;
			stream << arg;
			output += stream.str();
			break;
		case 'c':
			output.push_back((char)(arg & 0xFF));
			break;
		default:
			output.push_back('%');
			output.push_back(specifier);
			break;
		}
	}
	return EM_OK;
}

StepStatus scanStackArguments(MemoryManager& mm, const std::string& format, const std::string& input, DWORD esp, DWORD& assignments) {
	assignments = 0;
	std::istringstream inputStream(input);
	DWORD argIndex = 0;
	for (size_t i = 0; i + 1 < format.size(); ++i) {
		if (format[i] != '%')
			continue;
		char specifier = format[++i];
		if (specifier == '%')
			continue;
		if (!isFormatSpecifier(specifier))
			continue;

		DWORD destAddr = 0;
		StepStatus status = readStackArgument(mm, esp, argIndex++, destAddr);
		if (status < 0)
			return status;
		std::string token;
		if (specifier == 'c') {
			char ch = 0;
			if (!inputStream.get(ch))
				return EM_OK;
			status = mm.writeMem(destAddr, (BYTE)ch);
			if (status < 0)
				return status;
			++assignments;
			continue;
		}
		if (!(inputStream >> token))
			return EM_OK;
		try {
		switch (specifier) {
		case 's': {
			status = writeUtf8String(mm, destAddr, token);
			if (status < 0)
				return status;
			++assignments;
			break;
		}
		case 'd': {
			int value = std::stoi(token);
			status = mm.writeMem(destAddr, (DWORD)value);
			if (status < 0)
				return status;
			++assignments;
			break;
		}
		case 'u': {
			DWORD value = (DWORD)std::stoul(token);
			status = mm.writeMem(destAddr, value);
			if (status < 0)
				return status;
			++assignments;
			break;
		}
		case 'x':
		case 'X': {
			DWORD value = (DWORD)std::stoul(token, nullptr, 16);
			status = mm.writeMem(destAddr, value);
			if (status < 0)
				return status;
			++assignments;
			break;
		}
		default:
			break;
		}
		}
		catch (...) {
			return EM_OK;
		}
	}
	return EM_OK;
}

std::vector<std::string> splitDataValues(const std::string& values) {
	std::vector<std::string> result;
	std::string current;
	for (char sym : values) {
		if (sym == ',') {
			result.push_back(trim(current));
			current.clear();
		}
		else {
			current.push_back(sym);
		}
	}
	result.push_back(trim(current));
	return result;
}

bool parseDwordDataValue(const std::string& token, const std::vector<Label>& labels, DWORD& value) {
	std::string text = toLower(trim(token));
	if (text.empty())
		return false;

	std::smatch matches;
	std::regex offsetReg(R"(^offset +(\w+)$)");
	if (std::regex_match(text, matches, offsetReg))
		text = matches[1].str();

	for (const Label& label : labels) {
		if (label.name == text) {
			value = label.address;
			return true;
		}
	}

	try {
		size_t parsed = 0;
		if (text.size() > 1 && text.back() == 'h') {
			value = (DWORD)std::stoul(text.substr(0, text.size() - 1), &parsed, 16);
			return parsed == text.size() - 1;
		}
		value = (DWORD)std::stoul(text, &parsed, 10);
		return parsed == text.size();
	}
	catch (...) {
		return false;
	}
}
}

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
	case NOP:
		status = decodeNOP(instData);
		break;
	case OR:
		status = decodeOR(instData, 4);
		break;
	case SYSCALL:
		status = decodeSYSCALL(instData);
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
	case JB: case JNB: case JE: case JNE: case JBE: case JA:
	case JO: case JNO: case JS: case JNS: case JL: case JGE: case JLE: case JG:
		status = decodeRelativeJump(instData);
		//status = decodeJMP(instData);
		break;
	case SUB:
		status = decodeSUB(instData, 4);
		break;
	case ADD:
		status = decodeADD(instData, 4);
		break;
	case INC:
		status = decodeINC(instData, 4);
		break;
	case DEC:
		status = decodeDEC(instData, 4);
		break;
	case NOT:
		status = decodeNOT(instData, 4);
		break;
	case NEG:
		status = decodeNEG(instData, 4);
		break;
	case SHL: case SHR: case SAR:
		status = decodeShift(instData, 4);
		break;
	case MUL:
		status = decodeMUL(instData);
		break;
	case IMUL:
		status = decodeIMUL(instData);
		break;
	case DIV:
		status = decodeDIV(instData);
		break;
	case IDIV:
		status = decodeIDIV(instData);
		break;
	case TEST:
		status = decodeTEST(instData, 4);
		break;
	case MOV:
		status = decodeMOV(instData, 4);
		break;
	case LEA:
		status = decodeLEA(instData, 4);
		break;
	case XCHG:
		status = decodeXCHG(instData, 4);
		break;
	case CALL:
		status = decodeCALL(instData);
		break;
	case RET:
		status = decodeRET(instData);
		break;
	case CLC: case STC:
		status = decodeFlagInstruction(instData);
		break;
	case INT3:
		status = decodeINT3(instData);
		break;
	case JMP:
		status = decodeJMP(instData);
		break;
	case PUSH:
		status = decodePUSH(instData);
		break;
	case POP:
		status = decodePOP(instData);
		break;
	default:
		status = EM_INVALID_INSTRUCTION;
		break;
	}
	if (instData.instAddr + instData.instSize >= 0x8000'0000)
		status = EM_INVALID_ADDRESS;
	return status;
}

StepStatus InstructionManager::processInstruction(InstructionData instData) {
	StepStatus status = EM_OK;
	switch (instData.code) {
	case NOP:
		break;
	case OR:
		processOR(instData, 4);
		break;
	case SYSCALL:
		status = processSYSCALL(instData);
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
	case JBE:
		processJNA(instData);
		break;
	case JA:
		processJA(instData);
		break;
	case JO:
		processJO(instData);
		break;
	case JNO:
		processJNO(instData);
		break;
	case JS:
		processJS(instData);
		break;
	case JNS:
		processJNS(instData);
		break;
	case JL:
		processJL(instData);
		break;
	case JGE:
		processJGE(instData);
		break;
	case JLE:
		processJLE(instData);
		break;
	case JG:
		processJG(instData);
		break;
	case SUB:
		processSUB(instData, 4);
		break;
	case ADD:
		processADD(instData, 4);
		break;
	case INC:
		status = processINC(instData, 4);
		break;
	case DEC:
		status = processDEC(instData, 4);
		break;
	case NOT:
		status = processNOT(instData, 4);
		break;
	case NEG:
		status = processNEG(instData, 4);
		break;
	case SHL:
		status = processSHL(instData, 4);
		break;
	case SHR:
		status = processSHR(instData, 4);
		break;
	case SAR:
		status = processSAR(instData, 4);
		break;
	case MUL:
		status = processMUL(instData);
		break;
	case IMUL:
		status = processIMUL(instData);
		break;
	case DIV:
		status = processDIV(instData);
		break;
	case IDIV:
		status = processIDIV(instData);
		break;
	case TEST:
		processTEST(instData, 4);
		break;
	case MOV:
		processMOV(instData, 4);
		break;	
	case LEA:
		status = processLEA(instData, 4);
		break;
	case XCHG:
		status = processXCHG(instData, 4);
		break;
	case CALL:
		status = processCALL(instData);
		break;
	case RET:
		status = processRET(instData);
		break;
	case CLC:
		mm.ctx.EFLAGS[EFLAG_CF] = 0;
		break;
	case STC:
		mm.ctx.EFLAGS[EFLAG_CF] = 1;
		break;
	case INT3:
		status = EM_BREAKPOINT;
		break;
	case JMP:
		processJMP(instData);
		break;
	case PUSH:
		status = processPUSH(instData);
		break;
	case POP:
		status = processPOP(instData);
		break;
	}
	if (status >= 0)
		mm.ctx.EIP += instData.instSize;
	return status;
}

StepStatus InstructionManager::parseCode(const std::vector<std::string>& lines, std::vector<BYTE>& bytes, std::vector<std::pair<int, StepStatus>>& errors) {
	StepStatus status = EM_OK;
	std::vector<Label> labels = getLabels(lines);
	setLabelsAddrs(labels, lines);
	for (int i = 0; i < lines.size();++i) {
		status = parseLine(lines[i], bytes, labels);
		if (status != EM_OK)
			errors.push_back(std::pair<int, StepStatus>(i + 1,status));
	}
	if (errors.size() >= 1)
		status = errors[0].second;
	return status;
}

std::vector<Label> InstructionManager::getLabels(const std::vector<std::string>& lines) {
	std::vector<Label> labels;
	std::smatch matches;
	std::regex reg(R"(^ *(\w+): *(?:$|\/\/[\w\W]*))");
	std::regex dataReg(R"(^ *(\w+) +(db|dd) +)");
	for (int i = 0; i < lines.size();++i) {
		if (std::regex_search(lines[i], matches, reg)) {
			Label label;
			label.name = matches[1].str();
			label.line = i;
			label.address = 0;
			labels.push_back(label);
		}
		else if (std::regex_search(lines[i], matches, dataReg)) {
			Label label;
			label.name = matches[1].str();
			label.line = i;
			label.address = 0;
			labels.push_back(label);
		}
	}
	return labels;
}

void InstructionManager::setLabelsAddrs(std::vector<Label>& labels, const std::vector<std::string>& lines) {
	std::vector<BYTE> bytes;
	for (int i = 0; i < lines.size(); ++i) {
		for (Label& label : labels)
			if (label.line == i)
				label.address = mm.ctx.EIP + bytes.size();
		parseLine(lines[i], bytes, labels);
	}
}

bool InstructionManager::isLabel(const std::string& line) {
	const std::regex reg(R"(^ *(\w+): *(?:$|\/\/[\w\W]*))");
	return std::regex_match(line, reg);
}

bool InstructionManager::isDataLine(const std::string& line) {
	const std::regex dbReg("^ *(?:(\\w+) +)?db +\"((?:\\\\.|[^\"])*)\" *(?:$|//[\\w\\W]*)");
	const std::regex ddReg(R"(^ *(?:(\w+) +)?dd +(.+?) *(?:$|\/\/[\w\W]*))");
	return std::regex_match(line, dbReg) || std::regex_match(line, ddReg);
}

StepStatus InstructionManager::parseLine(const std::string& line, std::vector<BYTE>& bytesOut, const std::vector<Label>& labels) {
	if (isEmptyLine(line) || isLabel(line))
		return EM_OK;
	if (isDataLine(line))
		return parseDataLine(line, bytesOut, labels);

	std::vector<BYTE> bytes;
	std::smatch matches;
	std::regex reg3(R"(^([a-z|A-Z]+) +([ \+\*\w\[\]]+), *([ \+\*\w\[\]]+), *([ \+\*\w\[\]]+) *(?:$|\/\/[\w\W]*))");
	std::regex reg2(R"(^([a-z|A-Z]+) +([ \+\*\w\[\]]+), *([ \+\*\w\[\]]+) *(?:$|\/\/[\w\W]*))");
	std::regex reg1(R"(^([a-z|A-Z]+) +([ \+\*\w\[\]]+) *(?:$|\/\/[\w\W]*))");
	std::regex reg0(R"(^([a-z|A-Z|3]+) *(?:$|\/\/[\w\W]*))");
	if(!std::regex_search(line, matches, reg3))
		if(!std::regex_search(line, matches, reg2))
			if(!std::regex_search(line, matches, reg1))
				if(!std::regex_search(line, matches, reg0))
					return EM_INVALID_SYNTAX;
	bytes.push_back(parseOpcode(matches[1].str()));
	if (bytes[0] == 0)
		return EM_INVALID_INSTRUCTION;
	for (int i = 2; i < matches.size(); ++i) {
		std::vector<BYTE> operandBytes = parseOperand(matches[i].str(), labels);
		if (operandBytes[0] == 0)
			return EM_INVALID_OPERAND;
		bytes.insert(bytes.end(),operandBytes.begin(),operandBytes.end());
	}
	StepStatus status = validateInstruction(bytes);
	if (status == EM_BREAKPOINT)
		status = EM_OK;
	if(status == EM_OK)
		bytesOut.insert(bytesOut.end(), bytes.begin(), bytes.end());
	return status;
}

StepStatus InstructionManager::parseDataLine(const std::string& line, std::vector<BYTE>& bytes, const std::vector<Label>& labels) {
	std::smatch matches;
	const std::regex dbReg("^ *(?:(\\w+) +)?db +\"((?:\\\\.|[^\"])*)\" *(?:$|//[\\w\\W]*)");
	if (std::regex_match(line, matches, dbReg)) {
		std::string decoded = parseStringLiteral(matches[2].str());
		bytes.insert(bytes.end(), decoded.begin(), decoded.end());
		bytes.push_back(0);
		return EM_OK;
	}

	const std::regex ddReg(R"(^ *(?:(\w+) +)?dd +(.+?) *(?:$|\/\/[\w\W]*))");
	if (!std::regex_match(line, matches, ddReg))
		return EM_INVALID_SYNTAX;

	for (const std::string& token : splitDataValues(matches[2].str())) {
		DWORD value = 0;
		if (!parseDwordDataValue(token, labels, value))
			return EM_INVALID_SYNTAX;
		BYTE* valueBytes = reinterpret_cast<BYTE*>(&value);
		bytes.insert(bytes.end(), valueBytes, valueBytes + sizeof(DWORD));
	}
	return EM_OK;
}

std::string InstructionManager::parseStringLiteral(const std::string& str) {
	std::string result;
	for (size_t i = 0; i < str.size(); ++i) {
		if (str[i] != '\\' || i + 1 >= str.size()) {
			result.push_back(str[i]);
			continue;
		}
		char escaped = str[++i];
		switch (escaped) {
		case 'n':
			result.push_back('\n');
			break;
		case 'r':
			result.push_back('\r');
			break;
		case 't':
			result.push_back('\t');
			break;
		case '0':
			result.push_back('\0');
			break;
		case '\\':
		case '"':
			result.push_back(escaped);
			break;
		default:
			result.push_back(escaped);
			break;
		}
	}
	return result;
}

BYTE InstructionManager::parseOpcode(std::string opcodeStr) {
	for (char& sym : opcodeStr) sym = std::tolower(sym);
	const std::map<std::string, Opcode>& opcodeDict = getOpcodeDict();
	if (!opcodeDict.contains(opcodeStr))
		return 0;
	return opcodeDict.at(opcodeStr);
}

std::string InstructionManager::parseLabel(const std::vector<Label>& labels, const std::string& labelName) {
	std::string result = "";
	for (const Label& label : labels) {
		if (label.name == labelName) {
			result = std::to_string(label.address);
		}
	}
	return result;
}

std::vector<BYTE> InstructionManager::parseOperand(std::string operandStr, const std::vector<Label>& labels) {
	for (char& sym : operandStr) sym = std::tolower(sym);
	std::smatch matches;
	std::regex offsetReg(R"(^ *offset +(\w+) *$)");
	if (std::regex_match(operandStr, matches, offsetReg)) {
		std::string num = parseLabel(labels, matches[1].str());
		if (num == "")
			return std::vector<BYTE>{ 0 };
		return parseNum(num);
	}
	std::regex reg(R"((\[[\w+*]+\])|([\w]+))");
	if (std::regex_search(operandStr, matches, reg)) {
		if (matches[1].matched)
			operandStr = matches[1].str();
		else
			operandStr = matches[2].str();
		
	}
	
	std::vector<BYTE> bytes;
	if (isEffAddr((operandStr)))
		return parseEffAddr(operandStr, labels);
	else {
		if (isNumber(operandStr))
			bytes = parseNum(operandStr);
		else {
			const std::map<std::string, OperandType>& operandDict = getOperandDict();
			if (operandDict.contains(operandStr))
				bytes.push_back(operandDict.at(operandStr));
			else {
				std::string num = parseLabel(labels, operandStr);
				if (num == "")
					bytes.push_back(0);
				else
					bytes = parseNum(num);
			}
		}
	}
	return bytes;
}



bool InstructionManager::isEffAddr(const std::string& operandStr){
	const std::regex reg(R"(\[[\w +*]+\])");
	return std::regex_match(operandStr, reg);
}

std::vector<BYTE> InstructionManager::parseEffAddr(const std::string& operandStr, const std::vector<Label>& labels) {
	std::vector<BYTE> bytes;
	std::regex effReg3(R"(\[ *(\w+) *\* *(\w+) *\])");
	std::regex effReg2(R"(\[ *(\w+) *\+ *(\w+) *\])");
	std::regex effReg1(R"(\[ *(\w+) *\])");
	std::smatch matches;
	if (std::regex_search(operandStr, matches, effReg3)) {
		bytes = parseOperand(matches[1].str(), labels);
		std::vector<BYTE> bytes2 = parseOperand(matches[2].str(), labels);
		if (bytes[0] != 0 && bytes2[0] != 0 && !isNumber(matches[1].str())) {
			if (isNumber(matches[2].str())) {
				bytes[0] |= EAD_MUL32_READ << 4;
				bytes.insert(bytes.end(), ++bytes2.begin(), bytes2.end());
			}
			else {
				bytes[0] |= EAD_MULREG_READ << 4;
				bytes.insert(bytes.end(), bytes2.begin(), bytes2.end());
			}
		}
		else
			bytes[0] = 0;

	}
	else if (std::regex_search(operandStr, matches, effReg2)) {
		bytes = parseOperand(matches[1].str(), labels);
		std::vector<BYTE> bytes2 = parseOperand(matches[2].str(), labels);
		if (bytes[0] != 0 && bytes2[0] != 0 && !isNumber(matches[1].str())) {
			if (isNumber(matches[2].str())) {
				bytes[0] |= EAD_ADD32_READ << 4;
				bytes.insert(bytes.end(), ++bytes2.begin(), bytes2.end());
			}
			else {
				bytes[0] |= EAD_ADDREG_READ << 4;
				bytes.insert(bytes.end(), bytes2.begin(), bytes2.end());
			}
		}
		else
			bytes[0] = 0;
	}
	else if (std::regex_search(operandStr, matches, effReg1)) {
		bytes = parseOperand(matches[1].str(), labels);
		if (bytes[0] != 0) {
			bytes[0] |= EAD_READ << 4;
		}
	}
	else bytes.push_back(0);
	return bytes;
}

bool InstructionManager::isNumber(const std::string& operandStr) {
	const std::regex reg(R"(^([\dabcdef]+h|\d+)$)");
	return std::regex_match(operandStr, reg);
}

std::vector<BYTE> InstructionManager::parseNum(const std::string& operandStr) {
	std::vector<BYTE> bytes;
	bytes.push_back(OT_RAWDATA);
	std::smatch matches;
	const std::regex regHex(R"(^([\dabcdef]+)h$)");
	const std::regex regDec(R"(^(\d+)$)");
	int num = 0;
	if (std::regex_search(operandStr, matches, regHex))
		num = std::stoi(matches[1].str(), 0, 16);
	else if (std::regex_search(operandStr, matches, regDec))
		num = std::stoi(matches[1].str(), 0, 10);
	else bytes[0] = 0;
	bytes.resize(5);
	memcpy(bytes.data() + 1, &num, sizeof(int));
	return bytes;
}

const std::map<std::string, Opcode>& InstructionManager::getOpcodeDict() {
	static const std::map<std::string, Opcode> opcodeDict{
		std::pair<const std::string, const Opcode> {"nop",NOP},
		std::pair<const std::string, const Opcode> {"or",OR},
		std::pair<const std::string, const Opcode> {"syscall",SYSCALL},
		std::pair<const std::string, const Opcode> {"and",AND},
		std::pair<const std::string, const Opcode> {"xor",XOR},
		std::pair<const std::string, const Opcode> {"cmp",CMP},
		std::pair<const std::string, const Opcode> {"jo",JO},
		std::pair<const std::string, const Opcode> {"jno",JNO},
		std::pair<const std::string, const Opcode> {"jb",JB},
		std::pair<const std::string, const Opcode> {"jnb",JNB},
		std::pair<const std::string, const Opcode> {"je",JE},
		std::pair<const std::string, const Opcode> {"jne",JNE},
		std::pair<const std::string, const Opcode> {"jbe",JBE},
		std::pair<const std::string, const Opcode> {"ja",JA},
		std::pair<const std::string, const Opcode> {"js",JS},
		std::pair<const std::string, const Opcode> {"jns",JNS},
		std::pair<const std::string, const Opcode> {"jl",JL},
		std::pair<const std::string, const Opcode> {"jge",JGE},
		std::pair<const std::string, const Opcode> {"jle",JLE},
		std::pair<const std::string, const Opcode> {"jg",JG},
		std::pair<const std::string, const Opcode> {"sub",SUB},
		std::pair<const std::string, const Opcode> {"add",ADD},
		std::pair<const std::string, const Opcode> {"inc",INC},
		std::pair<const std::string, const Opcode> {"dec",DEC},
		std::pair<const std::string, const Opcode> {"not",NOT},
		std::pair<const std::string, const Opcode> {"neg",NEG},
		std::pair<const std::string, const Opcode> {"shl",SHL},
		std::pair<const std::string, const Opcode> {"shr",SHR},
		std::pair<const std::string, const Opcode> {"sar",SAR},
		std::pair<const std::string, const Opcode> {"xchg",XCHG},
		std::pair<const std::string, const Opcode> {"lea",LEA},
		std::pair<const std::string, const Opcode> {"mul",MUL},
		std::pair<const std::string, const Opcode> {"imul",IMUL},
		std::pair<const std::string, const Opcode> {"div",DIV},
		std::pair<const std::string, const Opcode> {"idiv",IDIV},
		std::pair<const std::string, const Opcode> {"test",TEST},
		std::pair<const std::string, const Opcode> {"mov",MOV},
		std::pair<const std::string, const Opcode> {"call",CALL},
		std::pair<const std::string, const Opcode> {"ret",RET},
		std::pair<const std::string, const Opcode> {"clc",CLC},
		std::pair<const std::string, const Opcode> {"stc",STC},
		std::pair<const std::string, const Opcode> {"int3",INT3},
		std::pair<const std::string, const Opcode> {"jmp",JMP},
		std::pair<const std::string, const Opcode> {"push",PUSH},
		std::pair<const std::string, const Opcode> {"pop",POP}
	};
	return opcodeDict;
}

const std::map<std::string, OperandType>& InstructionManager::getOperandDict() {
	static const std::map<std::string, OperandType> operandDict{
		std::pair<const std::string, const OperandType> {"eax",OT_A},
		std::pair<const std::string, const OperandType> {"ebx",OT_B},
		std::pair<const std::string, const OperandType> {"ecx",OT_C},
		std::pair<const std::string, const OperandType> {"edx",OT_D},
		std::pair<const std::string, const OperandType> {"esp",OT_SP},
		std::pair<const std::string, const OperandType> {"ebp",OT_BP},
		std::pair<const std::string, const OperandType> {"esi",OT_SI},
		std::pair<const std::string, const OperandType> {"edi",OT_DI},
		std::pair<const std::string, const OperandType> {"eip",OT_IP}
	};
	return operandDict;
}

bool InstructionManager::isEmptyLine(const std::string& line) {
	const std::regex reg(R"(^ *(?:$|\/\/[\w\W]*))");
	return std::regex_match(line,reg);
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
	if (*status < 0 || op.mainType > OT_RAWDATA || op.mainType == OT_NONE) {
		if (*status >= 0)
			*status = EM_INVALID_OPERAND;
		return op;
	}

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

StepStatus InstructionManager::writeOperandValue(OperandData op, DWORD data, int size) {
	bool isEmulatedAddress = false;
	StepStatus status = EM_OK;
	void* addr = getOperandAddr(op, isEmulatedAddress, status);
	if (status < 0)
		return status;
	if (isEmulatedAddress) {
		switch (size) {
		case 1:
			return mm.writeMem((DWORD)addr, (BYTE)data);
		case 2:
			return mm.writeMem((DWORD)addr, (WORD)data);
		case 4:
			return mm.writeMem((DWORD)addr, data);
		default:
			return EM_INVALID_OPERAND;
		}
	}
	if (size == 1)
		*((BYTE*)addr + 3) = data;
	else if (size == 2)
		*((WORD*)addr + 2) = data;
	else if (size == 4)
		*(DWORD*)addr = data;
	else
		return EM_INVALID_OPERAND;
	return EM_OK;
}

StepStatus InstructionManager::validateInstruction(const std::vector<BYTE> bytes) {
	std::vector<BYTE> reserve(bytes.size() + 5);
	StepStatus status = EM_OK;
	for (int i = 0; i < reserve.size() && status == EM_OK; ++i) {
		BYTE byte;
		status = mm.readMem(mm.ctx.EIP + i, byte);
		reserve[i] = byte;
	}
	if (status != EM_OK)
		return status;
	for (int i = 0; i < bytes.size() + 5 && status == EM_OK; ++i) {
		if (i < bytes.size())
			status = mm.writeMem(mm.ctx.EIP + i, bytes[i]);
		else
			status = mm.writeMem(mm.ctx.EIP + i, BYTE(0));
	}
	
	InstructionData instData;
	if(status == EM_OK)
		status = decodeInstruction(instData);

	for (int i = 0; i < reserve.size(); ++i)
		mm.writeMem(mm.ctx.EIP + i, reserve[i]);

	return status;
}

DWORD InstructionManager::readOperand(OperandData op, int size) {
	DWORD result = 0;
	bool isEmulatedAddress = 0;
	StepStatus status = EM_OK;
	void* addr = getOperandAddr(op,isEmulatedAddress,status);
	if (isEmulatedAddress) {
		mm.readMem((DWORD)addr, result);
	}
	else {
		if (size == 1)
			result = *((BYTE*)addr + 3);
		else if (size == 2)
			result = *((WORD*)addr + 2);
		else if (size == 4)
			result = *(DWORD*)addr;
	}
	return result;
}

DWORD InstructionManager::calculateOperandAddressValue(OperandData op, StepStatus& status) {
	status = EM_OK;
	DWORD base = 0;
	switch (op.mainType) {
	case OT_A:
		base = mm.ctx.EAX;
		break;
	case OT_B:
		base = mm.ctx.EBX;
		break;
	case OT_C:
		base = mm.ctx.ECX;
		break;
	case OT_D:
		base = mm.ctx.EDX;
		break;
	case OT_SP:
		base = mm.ctx.ESP;
		break;
	case OT_BP:
		base = mm.ctx.EBP;
		break;
	case OT_SI:
		base = mm.ctx.ESI;
		break;
	case OT_DI:
		base = mm.ctx.EDI;
		break;
	case OT_IP:
		base = mm.ctx.EIP;
		break;
	case OT_RAWDATA:
		base = op.rawData;
		break;
	default:
		status = EM_INVALID_OPERAND;
		return 0;
	}

	switch (op.effAddr) {
	case EAD_DIRECT:
		return base;
	case EAD_READ:
		return base;
	case EAD_ADD8_READ:
	case EAD_ADD32_READ:
		return base + op.rawData;
	case EAD_MUL8_READ:
	case EAD_MUL32_READ:
		return base * op.rawData;
	case EAD_ADDREG_READ: {
		OperandData op2;
		op2.mainType = op.effAddrRegType;
		return base + readOperand(op2, 4);
	}
	case EAD_MULREG_READ: {
		OperandData op2;
		op2.mainType = op.effAddrRegType;
		return base * readOperand(op2, 4);
	}
	default:
		status = EM_INVALID_OPERAND;
		return 0;
	}
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

StepStatus InstructionManager::decodeNOP(InstructionData& instData) {
	instData.instSize = 1;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 1;
	return EM_OK;
}

StepStatus InstructionManager::decodeRET(InstructionData& instData) {
	instData.instSize = 0;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 4;
	return EM_OK;
}

StepStatus InstructionManager::decodeFlagInstruction(InstructionData& instData) {
	instData.instSize = 1;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 1;
	return EM_OK;
}

StepStatus InstructionManager::decodeSYSCALL(InstructionData& instData) {
	instData.instSize = 1;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 20;
	return EM_OK;
}

StepStatus InstructionManager::processSYSCALL(InstructionData instData) {
	constexpr DWORD SYSCALL_WRITE = 1;
	constexpr DWORD SYSCALL_READ = 2;
	constexpr DWORD SYSCALL_PRINTF = 3;
	constexpr DWORD SYSCALL_SCANF = 4;

	ProgramConsoleManager& console = ProgramConsoleManager::getProgramConsoleManager();
	if (mm.ctx.EAX == SYSCALL_WRITE) {
		std::string text;
		StepStatus status = readUtf8String(mm, mm.ctx.EDX, text, mm.ctx.ECX == 0 ? MAX_STRING_READ : mm.ctx.ECX);
		if (status < 0)
			return status;
		console.write(text);
		return EM_OK;
	}

	if (mm.ctx.EAX == SYSCALL_READ) {
		std::string input;
		console.beginInputRequest();
		if (!console.waitForInput(input))
			return EM_BREAKPOINT;
		StepStatus status = writeUtf8String(mm, mm.ctx.EDX, input);
		if (status < 0)
			return status;
		mm.ctx.EAX = (DWORD)input.size();
		return EM_OK;
	}

	if (mm.ctx.EAX == SYSCALL_PRINTF) {
		std::string format;
		StepStatus status = readUtf8String(mm, mm.ctx.ECX, format);
		if (status < 0)
			return status;
		std::string output;
		status = formatStackArguments(mm, format, mm.ctx.ESP, output);
		if (status < 0)
			return status;
		console.write(output);
		return EM_OK;
	}

	if (mm.ctx.EAX == SYSCALL_SCANF) {
		std::string format;
		StepStatus status = readUtf8String(mm, mm.ctx.ECX, format);
		if (status < 0)
			return status;
		std::string input;
		console.beginInputRequest();
		if (!console.waitForInput(input))
			return EM_BREAKPOINT;
		DWORD assignments = 0;
		status = scanStackArguments(mm, format, input, mm.ctx.ESP, assignments);
		if (status < 0)
			return status;
		mm.ctx.EAX = assignments;
		return EM_OK;
	}

	return EM_INVALID_OPERAND;
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

StepStatus InstructionManager::decodeLEA(InstructionData& instData, int opSize) {
	StepStatus status = decodeMOV(instData, opSize);
	if (status < 0)
		return status;
	if (instData.op2.effAddr == EAD_DIRECT)
		return EM_INVALID_OPERAND;
	instData.ticks = 2;
	return status;
}

StepStatus InstructionManager::processLEA(InstructionData instData, int opSize) {
	StepStatus status = EM_OK;
	DWORD addr = calculateOperandAddressValue(instData.op2, status);
	if (status < 0)
		return status;
	return writeOperandValue(instData.op1, addr, opSize);
}

StepStatus InstructionManager::decodeXCHG(InstructionData& instData, int opSize) {
	StepStatus status = decodeMOV(instData, opSize);
	if (status < 0)
		return status;
	if (instData.op2.mainType == OT_RAWDATA && instData.op2.effAddr == EAD_DIRECT)
		return EM_INVALID_OPERAND;
	instData.ticks = 3;
	return status;
}

StepStatus InstructionManager::processXCHG(InstructionData instData, int opSize) {
	DWORD left = readOperand(instData.op1, opSize);
	DWORD right = readOperand(instData.op2, opSize);
	StepStatus status = writeOperandValue(instData.op1, right, opSize);
	if (status < 0)
		return status;
	return writeOperandValue(instData.op2, left, opSize);
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
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, 4, &status, &lastOperandSize);
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
	if (mm.ctx.EFLAGS[EFLAG_CF]) {
		//mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJNB(InstructionData instData) {
	if (!mm.ctx.EFLAGS[EFLAG_CF]) {
		//mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJE(InstructionData instData) {
	if (mm.ctx.EFLAGS[EFLAG_ZF]) {
		//mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJNE(InstructionData instData) {
	if (!mm.ctx.EFLAGS[EFLAG_ZF]) {
		//mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJNA(InstructionData instData) {
	if (mm.ctx.EFLAGS[EFLAG_CF] || mm.ctx.EFLAGS[EFLAG_ZF]) {
		//mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJA(InstructionData instData) {
	if (!mm.ctx.EFLAGS[EFLAG_CF] && !mm.ctx.EFLAGS[EFLAG_ZF]) {
		//mm.ctx.EIP = mm.ctx.EIP + *(short*)&instData.op1.rawData;
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJO(InstructionData instData) {
	if (mm.ctx.EFLAGS[EFLAG_OF]) {
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJNO(InstructionData instData) {
	if (!mm.ctx.EFLAGS[EFLAG_OF]) {
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJS(InstructionData instData) {
	if (mm.ctx.EFLAGS[EFLAG_SF]) {
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJNS(InstructionData instData) {
	if (!mm.ctx.EFLAGS[EFLAG_SF]) {
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJL(InstructionData instData) {
	if (mm.ctx.EFLAGS[EFLAG_SF] != mm.ctx.EFLAGS[EFLAG_OF]) {
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJGE(InstructionData instData) {
	if (mm.ctx.EFLAGS[EFLAG_SF] == mm.ctx.EFLAGS[EFLAG_OF]) {
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJLE(InstructionData instData) {
	if (mm.ctx.EFLAGS[EFLAG_ZF] || mm.ctx.EFLAGS[EFLAG_SF] != mm.ctx.EFLAGS[EFLAG_OF]) {
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
}

void InstructionManager::processJG(InstructionData instData) {
	if (!mm.ctx.EFLAGS[EFLAG_ZF] && mm.ctx.EFLAGS[EFLAG_SF] == mm.ctx.EFLAGS[EFLAG_OF]) {
		processJMP(instData);
		mm.ctx.EIP -= instData.instSize;
	}
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
	mm.ctx.EFLAGS[EFLAG_CF] = destData < srcData; 
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
		result = srcData + destData;
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

StepStatus InstructionManager::decodeINC(InstructionData& instData, int opSize) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, opSize, &status, &lastOperandSize);
	if (status < 0)
		return status;
	if (instData.op1.mainType == OT_RAWDATA && instData.op1.effAddr == EAD_DIRECT)
		return EM_INVALID_OPERAND;
	counter += lastOperandSize;
	instData.instSize = counter;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 1;
	return status;
}

StepStatus InstructionManager::processINC(InstructionData instData, int opSize) {
	bool oldCF = mm.ctx.EFLAGS[EFLAG_CF];
	DWORD value = readOperand(instData.op1, opSize);
	DWORD result = value + 1;
	StepStatus status = writeOperandValue(instData.op1, result, opSize);
	if (status < 0)
		return status;
	mm.ctx.EFLAGS[EFLAG_CF] = oldCF;
	mm.ctx.EFLAGS[EFLAG_OF] = value == 0x7FFF'FFFF;
	mm.ctx.EFLAGS[EFLAG_SF] = result & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !result;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(result);
	return EM_OK;
}

StepStatus InstructionManager::decodeDEC(InstructionData& instData, int opSize) {
	StepStatus status = decodeINC(instData, opSize);
	if (status == EM_OK)
		instData.ticks = 1;
	return status;
}

StepStatus InstructionManager::processDEC(InstructionData instData, int opSize) {
	bool oldCF = mm.ctx.EFLAGS[EFLAG_CF];
	DWORD value = readOperand(instData.op1, opSize);
	DWORD result = value - 1;
	StepStatus status = writeOperandValue(instData.op1, result, opSize);
	if (status < 0)
		return status;
	mm.ctx.EFLAGS[EFLAG_CF] = oldCF;
	mm.ctx.EFLAGS[EFLAG_OF] = value == 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_SF] = result & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !result;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(result);
	return EM_OK;
}

StepStatus InstructionManager::decodeNOT(InstructionData& instData, int opSize) {
	StepStatus status = decodeINC(instData, opSize);
	if (status == EM_OK)
		instData.ticks = 1;
	return status;
}

StepStatus InstructionManager::processNOT(InstructionData instData, int opSize) {
	DWORD value = readOperand(instData.op1, opSize);
	return writeOperandValue(instData.op1, ~value, opSize);
}

StepStatus InstructionManager::decodeNEG(InstructionData& instData, int opSize) {
	StepStatus status = decodeINC(instData, opSize);
	if (status == EM_OK)
		instData.ticks = 2;
	return status;
}

StepStatus InstructionManager::processNEG(InstructionData instData, int opSize) {
	DWORD value = readOperand(instData.op1, opSize);
	DWORD result = 0 - value;
	StepStatus status = writeOperandValue(instData.op1, result, opSize);
	if (status < 0)
		return status;
	mm.ctx.EFLAGS[EFLAG_CF] = value != 0;
	mm.ctx.EFLAGS[EFLAG_OF] = value == 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_SF] = result & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !result;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(result);
	return EM_OK;
}

StepStatus InstructionManager::decodeShift(InstructionData& instData, int opSize) {
	StepStatus status = decodeADD(instData, opSize);
	if (status == EM_OK)
		instData.ticks = 2;
	return status;
}

StepStatus InstructionManager::processSHL(InstructionData instData, int opSize) {
	DWORD value = readOperand(instData.op1, opSize);
	DWORD count = readOperand(instData.op2, opSize) & 0x1F;
	DWORD result = value;
	if (count > 0) {
		mm.ctx.EFLAGS[EFLAG_CF] = (value >> (32 - count)) & 1;
		result = value << count;
		mm.ctx.EFLAGS[EFLAG_OF] = count == 1 ? (((result >> 31) & 1) != mm.ctx.EFLAGS[EFLAG_CF]) : 0;
	}
	StepStatus status = writeOperandValue(instData.op1, result, opSize);
	if (status < 0)
		return status;
	mm.ctx.EFLAGS[EFLAG_SF] = result & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !result;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(result);
	return EM_OK;
}

StepStatus InstructionManager::processSHR(InstructionData instData, int opSize) {
	DWORD value = readOperand(instData.op1, opSize);
	DWORD count = readOperand(instData.op2, opSize) & 0x1F;
	DWORD result = value;
	if (count > 0) {
		mm.ctx.EFLAGS[EFLAG_CF] = (value >> (count - 1)) & 1;
		result = value >> count;
		mm.ctx.EFLAGS[EFLAG_OF] = count == 1 ? (value & 0x8000'0000) != 0 : 0;
	}
	StepStatus status = writeOperandValue(instData.op1, result, opSize);
	if (status < 0)
		return status;
	mm.ctx.EFLAGS[EFLAG_SF] = result & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !result;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(result);
	return EM_OK;
}

StepStatus InstructionManager::processSAR(InstructionData instData, int opSize) {
	DWORD value = readOperand(instData.op1, opSize);
	DWORD count = readOperand(instData.op2, opSize) & 0x1F;
	DWORD result = value;
	if (count > 0) {
		mm.ctx.EFLAGS[EFLAG_CF] = (value >> (count - 1)) & 1;
		result = (DWORD)((int)value >> count);
		mm.ctx.EFLAGS[EFLAG_OF] = 0;
	}
	StepStatus status = writeOperandValue(instData.op1, result, opSize);
	if (status < 0)
		return status;
	mm.ctx.EFLAGS[EFLAG_SF] = result & 0x8000'0000;
	mm.ctx.EFLAGS[EFLAG_ZF] = !result;
	mm.ctx.EFLAGS[EFLAG_PF] = checkParityFlag(result);
	return EM_OK;
}

StepStatus InstructionManager::decodeCALL(InstructionData& instData) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, 4, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;
	instData.instSize = 0;
	instData.instAddr = mm.ctx.EIP;
	instData.op3.rawData = counter;
	instData.ticks = 6;
	return status;
}

StepStatus InstructionManager::processCALL(InstructionData instData) {
	DWORD returnAddress = instData.instAddr + instData.op3.rawData;
	StepStatus status = mm.writeMem((DWORD)mm.ctx.ESP, returnAddress);
	if (status < 0)
		return status;
	mm.ctx.ESP -= 4;
	processJMP(instData);
	return EM_OK;
}

StepStatus InstructionManager::processRET(InstructionData instData) {
	DWORD returnAddress = 0;
	StepStatus status = mm.readMem((DWORD)mm.ctx.ESP + 4, returnAddress);
	if (status < 0)
		return status;
	mm.ctx.ESP += 4;
	mm.ctx.EIP = returnAddress;
	return EM_OK;
}

StepStatus InstructionManager::decodeMUL(InstructionData& instData) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, 4, &status, &lastOperandSize);
	if (status < 0)
		return status;
	if (instData.op1.effAddr && instData.op1.mainType == OT_RAWDATA && instData.op1.effAddr == EAD_DIRECT)
		return EM_INVALID_OPERAND;
	counter += lastOperandSize;

	instData.instSize = counter;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 4;
	return status;
}

StepStatus InstructionManager::processMUL(InstructionData instData) {
	DWORD multiplier = readOperand(instData.op1, 4);
	unsigned long long result = (unsigned long long)mm.ctx.EAX * (unsigned long long)multiplier;
	mm.ctx.EAX = (DWORD)(result & 0xFFFF'FFFFull);
	mm.ctx.EDX = (DWORD)(result >> 32);
	mm.ctx.EFLAGS[EFLAG_CF] = mm.ctx.EDX != 0;
	mm.ctx.EFLAGS[EFLAG_OF] = mm.ctx.EFLAGS[EFLAG_CF];
	return EM_OK;
}

StepStatus InstructionManager::decodeIMUL(InstructionData& instData) {
	StepStatus status = decodeMUL(instData);
	if (status == EM_OK)
		instData.ticks = 5;
	return status;
}

StepStatus InstructionManager::processIMUL(InstructionData instData) {
	int multiplier = (int)readOperand(instData.op1, 4);
	long long result = (long long)(int)mm.ctx.EAX * (long long)multiplier;
	mm.ctx.EAX = (DWORD)(result & 0xFFFF'FFFFll);
	mm.ctx.EDX = (DWORD)(((unsigned long long)result) >> 32);
	mm.ctx.EFLAGS[EFLAG_CF] = result < (std::numeric_limits<int>::min)() || result > (std::numeric_limits<int>::max)();
	mm.ctx.EFLAGS[EFLAG_OF] = mm.ctx.EFLAGS[EFLAG_CF];
	return EM_OK;
}

StepStatus InstructionManager::decodeDIV(InstructionData& instData) {
	StepStatus status = decodeMUL(instData);
	if (status < 0)
		return status;
	instData.ticks = 8;
	return status;
}

StepStatus InstructionManager::processDIV(InstructionData instData) {
	DWORD divisor = readOperand(instData.op1, 4);
	if (divisor == 0)
		return EM_INVALID_OPERAND;
	unsigned long long dividend = ((unsigned long long)mm.ctx.EDX << 32) | mm.ctx.EAX;
	if (dividend / divisor > 0xFFFF'FFFFull)
		return EM_INVALID_OPERAND;
	mm.ctx.EAX = (DWORD)(dividend / divisor);
	mm.ctx.EDX = (DWORD)(dividend % divisor);
	return EM_OK;
}

StepStatus InstructionManager::decodeIDIV(InstructionData& instData) {
	StepStatus status = decodeMUL(instData);
	if (status < 0)
		return status;
	instData.ticks = 9;
	return status;
}

StepStatus InstructionManager::processIDIV(InstructionData instData) {
	int divisor = (int)readOperand(instData.op1, 4);
	if (divisor == 0)
		return EM_INVALID_OPERAND;
	long long dividend = ((long long)(int)mm.ctx.EDX << 32) | mm.ctx.EAX;
	long long quotient = dividend / divisor;
	if (quotient < (std::numeric_limits<int>::min)() || quotient > (std::numeric_limits<int>::max)())
		return EM_INVALID_OPERAND;
	mm.ctx.EAX = (DWORD)(int)quotient;
	mm.ctx.EDX = (DWORD)(int)(dividend % divisor);
	return EM_OK;
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

StepStatus InstructionManager::decodePUSH(InstructionData& instData) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, 4, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;

	instData.instSize = counter;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 2;
	return status;
}

StepStatus InstructionManager::processPUSH(InstructionData& instData) {
	bool isSrcEmulated = false;
	StepStatus status = EM_OK;
	DWORD srcData = 0;
	void* src = getOperandAddr(instData.op1, isSrcEmulated, status);
	//void* dest = getOperandAddr(instData.op1, isDestEmulated, status);
	if (isSrcEmulated)
		mm.readMem((DWORD)src, srcData);
	else
		srcData = *(DWORD*)src;
	status = mm.writeMem((DWORD)mm.ctx.ESP, srcData);
	if (status >= 0)
		mm.ctx.ESP -= 4;
	return status;
}

StepStatus InstructionManager::decodePOP(InstructionData& instData) {
	StepStatus status = EM_OK;
	int counter = 1;
	int lastOperandSize = 0;
	instData.op1 = decodeOperand(mm.ctx.EIP + counter, 4, &status, &lastOperandSize);
	if (status < 0)
		return status;
	counter += lastOperandSize;

	instData.instSize = counter;
	instData.instAddr = mm.ctx.EIP;
	instData.ticks = 2;
	return status;
}

StepStatus InstructionManager::processPOP(InstructionData& instData) {
	bool isDestEmulated = false, isSrcEmulated = false;
	StepStatus status = EM_OK;
	DWORD srcData = 0;
	void* dest = getOperandAddr(instData.op1, isDestEmulated, status);
	status = mm.readMem((DWORD)mm.ctx.ESP + 4, srcData);
	if (status >= 0 && isDestEmulated)
		mm.writeMem((DWORD)dest, srcData);
	else if(status >= 0)
		*(DWORD*)dest = srcData;
	if (status >= 0)
		mm.ctx.ESP += 4;
	return status;
}
