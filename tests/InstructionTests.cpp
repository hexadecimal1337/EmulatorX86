#include "Logic/InstructionManager.h"
#include "Logic/MemoryManager.h"
#include "Logic/ProgramConsoleManager.h"

#include <iostream>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr DWORD kCodeBase = 0x0040'1000;
constexpr DWORD kDataAddr = 0x0042'0000;

struct TestResult {
	const char* name;
	bool passed;
	std::string message;
};

void resetMachine() {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	mm.reset();
	mm.ctx.EIP = kCodeBase;
	mm.ctx.frequency = 1'000'000;
	ProgramConsoleManager::getProgramConsoleManager().clear();
}

StepStatus compileProgram(const std::vector<std::string>& lines) {
	InstructionManager& im = InstructionManager::getInstructionManager();
	MemoryManager& mm = MemoryManager::getMemoryManager();
	std::vector<BYTE> code;
	std::vector<std::pair<int, StepStatus>> errors;
	StepStatus status = im.parseCode(lines, code, errors);
	if (status != EM_OK)
		return status;
	for (size_t i = 0; i < code.size(); ++i)
		mm.writeMem(mm.ctx.EIP + static_cast<DWORD>(i), code[i]);
	return EM_OK;
}

StepStatus runProgram(int maxInstructions = 128) {
	InstructionManager& im = InstructionManager::getInstructionManager();
	MemoryManager& mm = MemoryManager::getMemoryManager();
	for (int i = 0; i < maxInstructions; ++i) {
		InstructionData instData;
		StepStatus status = im.decodeInstruction(instData);
		if (status < 0)
			return status;
		mm.ctx.counter += instData.ticks;
		status = im.processInstruction(instData);
		if (status < 0)
			return status;
		if (status == EM_BREAKPOINT)
			return EM_OK;
	}
	return EM_INVALID_ADDRESS;
}

TestResult runCase(const char* name, const std::vector<std::string>& lines, bool (*check)(std::string&)) {
	resetMachine();
	StepStatus status = compileProgram(lines);
	if (status != EM_OK)
		return { name, false, "compile failed: " + std::to_string(status) };
	status = runProgram();
	if (status != EM_OK)
		return { name, false, "run failed: " + std::to_string(status) };
	std::string message;
	bool passed = check(message);
	return { name, passed, message };
}

TestResult runCaseWithSetup(const char* name, const std::vector<std::string>& lines, void (*setup)(), bool (*check)(std::string&)) {
	resetMachine();
	setup();
	StepStatus status = compileProgram(lines);
	if (status != EM_OK)
		return { name, false, "compile failed: " + std::to_string(status) };
	status = runProgram();
	if (status != EM_OK)
		return { name, false, "run failed: " + std::to_string(status) };
	std::string message;
	bool passed = check(message);
	return { name, passed, message };
}

TestResult runCaseWithConsoleInput(const char* name, const std::vector<std::string>& lines, const std::string& input, bool (*check)(std::string&)) {
	resetMachine();
	StepStatus status = compileProgram(lines);
	if (status != EM_OK)
		return { name, false, "compile failed: " + std::to_string(status) };

	ProgramConsoleManager& console = ProgramConsoleManager::getProgramConsoleManager();
	std::thread inputThread([&console, input]() {
		while (!console.isInputWaiting())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		console.pushInput(input);
	});
	status = runProgram();
	inputThread.join();
	if (status != EM_OK)
		return { name, false, "run failed: " + std::to_string(status) };
	std::string message;
	bool passed = check(message);
	return { name, passed, message };
}

bool expectMovAdd(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 12) {
		message = "EAX expected 12, got " + std::to_string(mm.ctx.EAX);
		return false;
	}
	if (!mm.ctx.EFLAGS[EFLAG_PF]) {
		message = "PF expected true for result 12";
		return false;
	}
	return true;
}

bool expectSubBorrow(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 0xFFFF'FFFF) {
		message = "EAX expected 0xFFFFFFFF";
		return false;
	}
	if (!mm.ctx.EFLAGS[EFLAG_CF]) {
		message = "CF expected true after 1 - 2";
		return false;
	}
	if (!mm.ctx.EFLAGS[EFLAG_SF]) {
		message = "SF expected true after negative result";
		return false;
	}
	return true;
}

bool expectJbeEqual(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.ECX != 2) {
		message = "ECX expected 2 when JBE is taken on equality";
		return false;
	}
	return true;
}

bool expectMemoryAdd(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	DWORD value = 0;
	mm.readMem(kDataAddr, value);
	if (value != 12) {
		message = "memory value expected 12, got " + std::to_string(value);
		return false;
	}
	if (mm.ctx.ECX != 12) {
		message = "ECX expected 12 after reading memory";
		return false;
	}
	return true;
}

bool expectPushPop(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EBX != 1234) {
		message = "EBX expected 1234 after push/pop";
		return false;
	}
	if (mm.ctx.ESP != 0x7FFF'0000) {
		message = "ESP expected to be restored";
		return false;
	}
	return true;
}

bool expectLogicAndTest(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 0) {
		message = "EAX expected 0 after xor eax, eax";
		return false;
	}
	if (!mm.ctx.EFLAGS[EFLAG_ZF]) {
		message = "ZF expected true after zero result";
		return false;
	}
	return true;
}

bool expectMul(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 42 || mm.ctx.EDX != 0) {
		message = "EDX:EAX expected 0:42 after unsigned multiply";
		return false;
	}
	if (mm.ctx.EFLAGS[EFLAG_CF] || mm.ctx.EFLAGS[EFLAG_OF]) {
		message = "CF/OF expected false when high half is zero";
		return false;
	}
	return true;
}

bool expectImul(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if ((int)mm.ctx.EAX != -42 || mm.ctx.EDX != 0xFFFF'FFFF) {
		message = "EDX:EAX expected signed -42 after imul";
		return false;
	}
	if (mm.ctx.EFLAGS[EFLAG_CF] || mm.ctx.EFLAGS[EFLAG_OF]) {
		message = "CF/OF expected false when signed result fits in EAX";
		return false;
	}
	return true;
}

bool expectDiv(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 14 || mm.ctx.EDX != 2) {
		message = "EAX/rem expected 14/2 after div";
		return false;
	}
	return true;
}

bool expectIdiv(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if ((int)mm.ctx.EAX != -14 || (int)mm.ctx.EDX != -2) {
		message = "EAX/rem expected -14/-2 after idiv";
		return false;
	}
	return true;
}

bool expectCounter64(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.counter <= 0xFFFF'FFFFull) {
		message = "counter expected to stay above 32-bit range";
		return false;
	}
	return true;
}

bool expectIncDecNegNot(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 2) {
		message = "EAX expected 2 after inc/dec/not/neg";
		return false;
	}
	if (!mm.ctx.EFLAGS[EFLAG_CF]) {
		message = "CF expected true after neg non-zero";
		return false;
	}
	return true;
}

bool expectShiftOps(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 8) {
		message = "EAX expected 8 after shl";
		return false;
	}
	if (mm.ctx.EBX != 2) {
		message = "EBX expected 2 after shr";
		return false;
	}
	if ((int)mm.ctx.ECX != -4) {
		message = "ECX expected -4 after sar";
		return false;
	}
	return true;
}

bool expectLeaXchg(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 10 || mm.ctx.EBX != 0x0042'0004) {
		message = "LEA/XCHG expected EAX=10 and EBX=0x00420004";
		return false;
	}
	return true;
}

bool expectCallRet(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 1) {
		message = "EAX expected 1 after returning from function";
		return false;
	}
	if (mm.ctx.ESP != 0x7FFF'0000) {
		message = "ESP expected to be restored after call/ret";
		return false;
	}
	return true;
}

bool expectFlagJumps(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 7) {
		message = "EAX expected 7 after signed/overflow jump chain";
		return false;
	}
	return true;
}

bool expectCarryFlagOps(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 2) {
		message = "EAX expected 2 after stc/clc conditional jumps";
		return false;
	}
	if (mm.ctx.EFLAGS[EFLAG_CF]) {
		message = "CF expected false after clc";
		return false;
	}
	return true;
}

bool expectNopJleJge(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 3) {
		message = "EAX expected 3 after nop/jle/jge";
		return false;
	}
	return true;
}

bool expectSyscallUtf8Output(std::string& message) {
	std::string output = ProgramConsoleManager::getProgramConsoleManager().getOutput();
	std::string expected = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82!";
	if (output != expected) {
		message = "console output expected UTF-8 greeting";
		return false;
	}
	return true;
}

bool expectSyscallUtf8Input(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	std::string output = ProgramConsoleManager::getProgramConsoleManager().getOutput();
	std::string expected = "\xD0\x92\xD0\xB2\xD0\xBE\xD0\xB4";
	if (output != expected) {
		message = "console output expected echoed UTF-8 input";
		return false;
	}
	if (mm.ctx.EBX != expected.size()) {
		message = "EBX expected input byte length";
		return false;
	}
	return true;
}

bool expectPrintfScanf(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	std::string output = ProgramConsoleManager::getProgramConsoleManager().getOutput();
	DWORD left = 0;
	DWORD right = 0;
	mm.readMem(kDataAddr, left);
	mm.readMem(kDataAddr + 4, right);
	if (output != "left=123 right=456 text=ok") {
		message = "printf output expected two integers and one string, got " + output + ", memory left=" + std::to_string(left) + " right=" + std::to_string(right);
		return false;
	}
	if (left != 123 || right != 456) {
		message = "scanf expected memory values 123 and 456";
		return false;
	}
	if (mm.ctx.EBX != 2) {
		message = "scanf expected two assignments";
		return false;
	}
	if (mm.ctx.ESP != 0x7FFF'0000) {
		message = "ESP expected to be restored after stack syscall arguments";
		return false;
	}
	return true;
}

bool expectDdData(std::string& message) {
	MemoryManager& mm = MemoryManager::getMemoryManager();
	if (mm.ctx.EAX != 123456 || mm.ctx.EBX != 0xABCDEF) {
		message = "dd expected to emit 32-bit little-endian values, got EAX=" + std::to_string(mm.ctx.EAX) + " EBX=" + std::to_string(mm.ctx.EBX);
		return false;
	}
	return true;
}

TestResult expectIgnoredInputBeforeRequest() {
	resetMachine();
	ProgramConsoleManager::getProgramConsoleManager().pushInput("ignored");
	const std::vector<std::string> lines{
		"jmp main",
		"buffer db \"xxxxxxxxxxxxxxxx\"",
		"main:",
		"mov eax, 2",
		"mov edx, offset buffer",
		"syscall",
		"mov edx, offset buffer",
		"mov ecx, 0",
		"mov eax, 1",
		"syscall",
		"int3"
	};
	StepStatus status = compileProgram(lines);
	if (status != EM_OK)
		return { "input before syscall is ignored", false, "compile failed: " + std::to_string(status) };
	ProgramConsoleManager& console = ProgramConsoleManager::getProgramConsoleManager();
	std::thread inputThread([&console]() {
		while (!console.isInputWaiting())
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		console.pushInput("actual");
	});
	status = runProgram();
	inputThread.join();
	if (status != EM_OK)
		return { "input before syscall is ignored", false, "run failed: " + std::to_string(status) };
	std::string output = console.getOutput();
	if (output != "actual")
		return { "input before syscall is ignored", false, "console should use input entered after syscall request" };
	return { "input before syscall is ignored", true, "" };
}

TestResult expectRuntimeError(const char* name, const std::vector<std::string>& lines, StepStatus expectedStatus) {
	resetMachine();
	StepStatus status = compileProgram(lines);
	if (status != EM_OK)
		return { name, false, "compile failed: " + std::to_string(status) };
	status = runProgram();
	if (status != expectedStatus)
		return { name, false, "expected status " + std::to_string(expectedStatus) + ", got " + std::to_string(status) };
	return { name, true, "" };
}
}

int main() {
	MemoryManager::getMemoryManager().init();

	const std::vector<TestResult> results{
		runCase("mov/add register",
			{ "mov eax, 5", "add eax, 7", "int3" },
			expectMovAdd),
		runCase("sub borrow flag",
			{ "mov eax, 1", "sub eax, 2", "int3" },
			expectSubBorrow),
		runCase("jbe on equality",
			{
				"mov eax, 5",
				"mov ebx, 5",
				"cmp eax, ebx",
				"jbe less_equal",
				"mov ecx, 1",
				"jmp done",
				"less_equal:",
				"mov ecx, 2",
				"done:",
				"int3"
			},
			expectJbeEqual),
		runCase("memory add",
			{
				"mov eax, 7",
				"mov ebx, 5",
				"mov [" + std::to_string(kDataAddr) + "], eax",
				"add [" + std::to_string(kDataAddr) + "], ebx",
				"mov ecx, [" + std::to_string(kDataAddr) + "]",
				"int3"
			},
			expectMemoryAdd),
		runCase("push/pop",
			{ "mov eax, 1234", "push eax", "pop ebx", "int3" },
			expectPushPop),
		runCase("xor/test flags",
			{ "mov eax, 1234", "xor eax, eax", "test eax, eax", "int3" },
			expectLogicAndTest),
		runCase("mul unsigned",
			{ "mov eax, 6", "mov ebx, 7", "mul ebx", "int3" },
			expectMul),
		runCase("imul signed",
			{ "mov eax, 6", "mov ebx, 0", "sub ebx, 7", "imul ebx", "int3" },
			expectImul),
		runCase("div unsigned",
			{ "mov edx, 0", "mov eax, 100", "mov ebx, 7", "div ebx", "int3" },
			expectDiv),
		runCase("idiv signed",
			{ "mov edx, 0", "sub edx, 1", "mov eax, 0", "sub eax, 100", "mov ebx, 7", "idiv ebx", "int3" },
			expectIdiv),
		runCase("64-bit counter",
			{ "int3" },
			[](std::string& message) {
				MemoryManager& mm = MemoryManager::getMemoryManager();
				mm.ctx.counter = 0xFFFF'FFFFull + 10;
				return expectCounter64(message);
			}),
		expectRuntimeError("div by zero",
			{ "mov edx, 0", "mov eax, 10", "mov ebx, 0", "div ebx", "int3" },
			EM_INVALID_OPERAND),
		runCase("inc/dec/not/neg",
			{ "mov eax, 1", "inc eax", "dec eax", "not eax", "neg eax", "int3" },
			expectIncDecNegNot),
		runCase("shift ops",
			{ "mov eax, 1", "shl eax, 3", "mov ebx, 8", "shr ebx, 2", "mov ecx, 0", "sub ecx, 8", "sar ecx, 1", "int3" },
			expectShiftOps),
		runCase("lea/xchg",
			{ "mov ebx, 4325376", "lea eax, [ebx+4]", "mov ebx, 10", "xchg eax, ebx", "int3" },
			expectLeaXchg),
		runCase("call/ret",
			{ "call fn", "mov eax, 1", "int3", "fn:", "mov eax, 2", "ret" },
			expectCallRet),
		runCase("signed and overflow jumps",
			{
				"mov eax, 0",
				"sub eax, 1",
				"js was_signed",
				"mov eax, 99",
				"was_signed:",
				"jns bad_signed",
				"mov eax, 1",
				"mov ebx, 2",
				"cmp eax, ebx",
				"jl less",
				"bad_signed:",
				"mov eax, 99",
				"less:",
				"mov eax, 2",
				"cmp ebx, eax",
				"jg greater",
				"mov eax, 99",
				"greater:",
				"mov eax, 1",
				"shl eax, 31",
				"neg eax",
				"jo overflowed",
				"mov eax, 99",
				"overflowed:",
				"jno bad_overflow",
				"mov eax, 7",
				"jmp done_jumps",
				"bad_overflow:",
				"mov eax, 99",
				"done_jumps:",
				"int3"
			},
			expectFlagJumps),
		runCase("stc/clc jumps",
			{
				"stc",
				"jb carry_set",
				"mov eax, 99",
				"carry_set:",
				"clc",
				"jnb carry_clear",
				"mov eax, 99",
				"carry_clear:",
				"mov eax, 2",
				"int3"
			},
			expectCarryFlagOps),
		runCase("nop/jle/jge",
			{
				"nop",
				"mov eax, 2",
				"mov ebx, 2",
				"cmp eax, ebx",
				"jle less_or_equal",
				"mov eax, 99",
				"less_or_equal:",
				"jge greater_or_equal",
				"mov eax, 99",
				"greater_or_equal:",
				"mov eax, 3",
				"int3"
			},
			expectNopJleJge),
		runCase("db string and syscall output",
			{
				"start:",
				"jmp main",
				std::string("my_string db \"") + "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82!" + "\"",
				"main:",
				"mov eax, 1",
				"mov ecx, 0",
				"mov edx, offset my_string",
				"syscall",
				"int3"
			},
			expectSyscallUtf8Output),
		runCaseWithConsoleInput("syscall input",
			{
				"jmp main",
				"buffer db \"xxxxxxxxxxxxxxxx\"",
				"main:",
				"mov eax, 2",
				"mov edx, offset buffer",
				"syscall",
				"mov ebx, eax",
				"mov edx, offset buffer",
				"mov ecx, 0",
				"mov eax, 1",
				"syscall",
				"int3"
			},
			"\xD0\x92\xD0\xB2\xD0\xBE\xD0\xB4",
			expectSyscallUtf8Input)
		,
		runCaseWithConsoleInput("printf/scanf",
			{
				"jmp main",
				"fmt_in db \"%d %d\"",
				"fmt_out db \"left=%d right=%d text=%s\"",
				"text db \"ok\"",
				"main:",
				"mov edx, " + std::to_string(kDataAddr + 4),
				"push edx",
				"mov edx, " + std::to_string(kDataAddr),
				"push edx",
				"mov eax, 4",
				"mov ecx, offset fmt_in",
				"syscall",
				"mov ebx, eax",
				"pop esi",
				"pop esi",
				"push offset text",
				"push [" + std::to_string(kDataAddr + 4) + "]",
				"push [" + std::to_string(kDataAddr) + "]",
				"mov eax, 3",
				"mov ecx, offset fmt_out",
				"syscall",
				"pop esi",
				"pop esi",
				"pop esi",
				"int3"
			},
			"123 456",
			expectPrintfScanf),
		runCase("dd data",
			{
				"jmp main",
				"value dd 123456",
				"other dd abcdefh",
				"main:",
				"mov eax, [value]",
				"mov ebx, [other]",
				"int3"
			},
			expectDdData),
		expectIgnoredInputBeforeRequest()
	};

	int failed = 0;
	for (const TestResult& result : results) {
		std::cout << (result.passed ? "[PASS] " : "[FAIL] ") << result.name;
		if (!result.message.empty())
			std::cout << " - " << result.message;
		std::cout << '\n';
		if (!result.passed)
			++failed;
	}

	MemoryManager::getMemoryManager().destroy();
	return failed == 0 ? 0 : 1;
}
