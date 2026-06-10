#include "EmulatorManager.h"
#include "ProgramConsoleManager.h"
#include <thread>
#include <chrono>

void EmulatorManager::init() {
    isInit.store(true);
    workThread = new std::thread(workFunction);
}

void EmulatorManager::destroy() {
    isInit.store(false);
    isNeedStop.store(false);
    ProgramConsoleManager::getProgramConsoleManager().cancelInput();
    if (workThread && workThread->joinable())
        workThread->join();
    delete workThread;
    workThread = nullptr;
}

void EmulatorManager::run() {
    isNeedStop.store(false);
}

void EmulatorManager::makeStepIn() {
    stepFlag.store(true);
    isNeedStop.store(false);
}

void EmulatorManager::stop() {
    isNeedStop.store(true);
    ProgramConsoleManager::getProgramConsoleManager().cancelInput();
}

const char* EmulatorManager::getStatusStr(StepStatus status) {
    const char* result = (const char*)u8"Неизвестная ошибка";
    switch (status) {
    case EM_OK:
        result = (const char*)u8"Успешное выполнение";
        break;
    case EM_INVALID_INSTRUCTION:
        result = (const char*)u8"Неизвестная инструкция\n\n      Неверный опкод\nПроверьте код команды";
        break;
    case EM_INVALID_OPERAND:
        result = (const char*)u8"Ошибка операнда\n\nОдин из операндов неверен\nПроверьте операнды/инструкцию";
        break;
    case EM_INVALID_ADDRESS:
        result = (const char*)u8"Ошибка доступа\n\nПопытка чтения/записи/выполнения в недопустимой памяти\nПроверьте EIP/операнды/инструкцию";
        break;
    }
    return result;
}

void EmulatorManager::checkState() {
    if (isNeedStop.load()) {
        isWorkingFlag.store(false);
        while(isNeedStop.load() && isInit.load())
            slowSpinlock(200);
    }
}

void EmulatorManager::spinlock(double seconds, std::atomic_bool* exitFlag) {
    auto startTime = std::chrono::high_resolution_clock::now();
    bool flag = true;
    while (flag) {
        std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - startTime;
        flag = elapsed.count() < seconds;
        if (exitFlag && exitFlag->load())
            break;
    }
}

void EmulatorManager::slowSpinlock(int milliSeconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliSeconds));
}

void EmulatorManager::workFunction() {
    EmulatorManager& em = EmulatorManager::getEmulatorManager();
    InstructionManager& im = InstructionManager::getInstructionManager();
    MemoryManager& mm = MemoryManager::getMemoryManager();
    while (em.isInit.load()) {
        em.checkState();
        if (!em.isNeedStop.load() && em.isInit.load()) {
            em.isWorkingFlag.store(true);
            StepStatus status = EM_OK;
            InstructionData instData;
            {
                std::lock_guard<std::recursive_mutex> cpuLock(mm.getStateMutex());
                status = im.decodeInstruction(instData);
            }
            if (status >= 0) {
                DWORD ticks = instData.ticks;
                int frequency = 1;
                {
                    std::lock_guard<std::recursive_mutex> cpuLock(mm.getStateMutex());
                    frequency = mm.ctx.frequency;
                }
                em.spinlock((double)ticks / (double)frequency, &em.isNeedStop);
                if (!em.isNeedStop.load()) {
                    if (instData.code == SYSCALL) {
                        {
                            std::lock_guard<std::recursive_mutex> cpuLock(mm.getStateMutex());
                            mm.ctx.counter += instData.ticks;
                        }
                        status = im.processInstruction(instData);
                    }
                    else {
                        std::lock_guard<std::recursive_mutex> cpuLock(mm.getStateMutex());
                        mm.ctx.counter += instData.ticks;
                        status = im.processInstruction(instData);
                    }
                    em.lastStatus.store(status);
                    if (em.stepFlag.load() || status == EM_BREAKPOINT) {
                        em.isNeedStop.store(true);
                        em.stepFlag.store(false);
                    }
                }
            }
            else {
                em.lastStatus.store(status);
                em.isNeedStop.store(true);
            }
        }
    }
}