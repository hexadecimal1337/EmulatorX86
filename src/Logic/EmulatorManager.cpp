#include "EmulatorManager.h"
#include <thread>
#include <chrono>

void EmulatorManager::init() {
    isInit = true;
    workThread = new std::thread(workFunction);
}

void EmulatorManager::destroy() {
    isInit = false;
    workThread->join();
    delete workThread;
    workThread = nullptr;
}

void EmulatorManager::run() {
    isNeedStop = false;
}

void EmulatorManager::makeStepIn() {
    stepFlag = true;
    isNeedStop = false;
}

void EmulatorManager::stop() {
    isNeedStop = true;
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
    if (isNeedStop) {
        isWorkingFlag = false;
        while(isNeedStop && isInit)
            slowSpinlock(200);
    }
}

void EmulatorManager::spinlock(double seconds, bool* exitFlag) {
    auto startTime = std::chrono::high_resolution_clock::now();
    bool flag = true;
    while (flag) {
        std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - startTime;
        flag = elapsed.count() < seconds;
        if (exitFlag && *exitFlag)
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
    while (em.isInit) {
        em.checkState();
        if (!em.isNeedStop && em.isInit) {
            em.isWorkingFlag = true;
            StepStatus status = EM_OK;
            InstructionData instData;
            status = im.decodeInstruction(instData);
            if (status >= 0) {
                em.spinlock((double)instData.ticks / (double)mm.ctx.frequency, &em.isNeedStop);
                if (!em.isNeedStop) {
                    mm.ctx.counter += instData.ticks;
                    im.processInstruction(instData);
                    em.lastStatus = status;
                    if (em.stepFlag || status == EM_BREAKPOINT) {
                        em.isNeedStop = true;
                        em.stepFlag = false;
                    }
                }
            }
            else {
                em.lastStatus = status;
                em.isNeedStop = true;
            }
        }
    }
}