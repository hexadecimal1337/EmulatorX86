#pragma once
#include "Structs.h"
#include <mutex>

class MemoryManager {
public:
	inline static MemoryManager& getMemoryManager() {
		static MemoryManager memoryManager;
		return memoryManager;
	}

	void init();
	void destroy();
	void optimize();
	bool saveToFile(const wchar_t* filePath);
	bool loadFromFile(const wchar_t* filePath);
	void reset();
	std::recursive_mutex& getStateMutex();

	template <typename T> StepStatus readMem(DWORD addr, T& data, ByteDirection BDir = BDir_Little_Endian);
	template <typename T> StepStatus writeMem(DWORD addr, T data, ByteDirection BDir = BDir_Little_Endian);

	int getEFLAGS();


	ThreadContext ctx = {};
private:
	static constexpr DWORD PAGE_SIZE = 0x1000;
	static constexpr DWORD PAGE_COUNT = 0xF'FFFF;
	static constexpr unsigned long long MEMORY_SIZE = (unsigned long long)PAGE_COUNT * PAGE_SIZE;

	static inline bool isAddressRangeValid(DWORD addr, unsigned long long size) {
		return size > 0 && (unsigned long long)addr < MEMORY_SIZE && (unsigned long long)addr + size <= MEMORY_SIZE;
	}

	BYTE readByte(DWORD page,WORD offset);
	void writeByte(DWORD page, WORD offset, BYTE byte);


	BYTE** memPages = nullptr;
	std::recursive_mutex stateMutex;
};

struct PageInfo {
	DWORD pageNum = 0;
	BYTE data[0x1000];
};

template<typename T>
inline StepStatus MemoryManager::readMem(DWORD addr, T& data, ByteDirection BDir) {
	std::lock_guard<std::recursive_mutex> lock(stateMutex);
	int size = sizeof(T);

	if (!isAddressRangeValid(addr, size))
		return EM_INVALID_ADDRESS;

	// Increase read speed if 1 byte
	if (size == 1) {
		data = (T)readByte(addr / PAGE_SIZE, addr % PAGE_SIZE);
		return EM_OK;
	}

	if (BDir == BDir_Little_Endian) {
		BYTE* dataPtr = (BYTE*)&data + size - 1;
		for (__int64 page = (addr + size - 1) / PAGE_SIZE; page >= 0; page--) {
			for (int offset = ((addr + size - 1) % PAGE_SIZE); offset >= 0 && size > 0; offset--, size--, dataPtr--) {
				*dataPtr = readByte(page, offset);
			}
		}
	}
	else if (BDir == BDir_Big_Endian) {
		BYTE* dataPtr = (BYTE*)&data;
		for (int page = ((__int64)addr) / PAGE_SIZE; page < PAGE_COUNT; page++) {
			for (int offset = (addr % PAGE_SIZE); offset < PAGE_SIZE && size > 0; offset++, dataPtr++, size--){
				*dataPtr = readByte(page, offset);
			}
		}
	}

	return EM_OK;
}

template<typename T>
inline StepStatus MemoryManager::writeMem(DWORD addr, T data, ByteDirection BDir) {
	std::lock_guard<std::recursive_mutex> lock(stateMutex);
	int size = sizeof(T);

	if (!isAddressRangeValid(addr, size))
		return EM_INVALID_ADDRESS;

	// Increase write speed if 1 byte
	if (size == 1) {
		writeByte(addr / PAGE_SIZE, addr % PAGE_SIZE, data);
		return EM_OK;
	}

	if (BDir == BDir_Little_Endian) {
		BYTE* dataPtr = (BYTE*)&data + size - 1;
		for (__int64 page = (addr + size - 1) / PAGE_SIZE; page >= 0; page--)
			for (int offset = ((addr + size - 1) % PAGE_SIZE); offset >= 0 && size > 0; offset--, size--, dataPtr--)
				writeByte(page, offset, *dataPtr);
	}
	else if (BDir == BDir_Big_Endian) {
		BYTE* dataPtr = (BYTE*)&data;
		for (int page = ((__int64)addr) / PAGE_SIZE; page < PAGE_COUNT; page++)
			for (int offset = (addr % PAGE_SIZE); offset < PAGE_SIZE && size > 0; offset++, dataPtr++, size--)
				writeByte(page, offset, *dataPtr);
	}
	else 
		return EM_INVALID_ADDRESS;
	return EM_OK;
}
