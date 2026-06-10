#pragma once

#include <Windows.h>
#include "InstructionManager.h"
#include <thread>
#include <mutex>
#include <atomic>
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
	//void makeStepDetour();
	//void makeStepOut();
	void stop();

	const char* getStatusStr(StepStatus status);
	inline bool isWorking() { return isWorkingFlag.load() || !isNeedStop.load(); };
	inline StepStatus getLastStatus() { return lastStatus.load(); };
	inline void resetStatus() { lastStatus.store(EM_OK); };
private:
	EmulatorManager() = default;

	void checkState();
	void spinlock(double seconds, std::atomic_bool* exitFlag = nullptr);
	void slowSpinlock(int milliSeconds);

	static void workFunction();

	InstructionManager& im = InstructionManager::getInstructionManager();
	std::atomic_bool isWorkingFlag = false;
	std::atomic_bool isNeedStop = true;
	std::atomic_bool isInit = false;
	std::atomic_bool stepFlag = false;
	std::atomic<StepStatus> lastStatus = EM_OK;
	std::thread* workThread = nullptr;
};
