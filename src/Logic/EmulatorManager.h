#pragma once

#include <Windows.h>
#include "InstructionManager.h"
#include <thread>
#include <mutex>
#include "MemoryManager.h"

class EmulatorManager {
public:
	inline static EmulatorManager& getEmulatorManager() {
		static EmulatorManager emulatorManager;
		return emulatorManager;
	}

	void init();
	void destroy();
	void run();
	void makeStepIn();
	void makeStepDetour();
	void makeStepOut();
	void stop();

	const char* getStatusStr(StepStatus status);
	inline bool isWorking() { return isWorkingFlag || !isNeedStop; };
	inline StepStatus getLastStatus() { return lastStatus; };
	inline void resetStatus() { lastStatus = EM_OK; };
private:
	EmulatorManager() = default;

	void checkState();
	void spinlock(double seconds, bool* exitFlag = nullptr);
	void slowSpinlock(int milliSeconds);

	static void workFunction();

	InstructionManager& im = InstructionManager::getInstructionManager();
	bool isWorkingFlag = false;
	bool isNeedStop = true;
	bool isInit = false;
	bool stepFlag = false;
	StepStatus lastStatus = EM_OK;
	std::thread* workThread = nullptr;
};