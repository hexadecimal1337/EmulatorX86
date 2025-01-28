#pragma once
#include "Structs.h"

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

	template <typename T> StepStatus readMem(DWORD addr, T& data, ByteDirection BDir = BDir_Little_Endian);
	template <typename T> StepStatus writeMem(DWORD addr, T data, ByteDirection BDir = BDir_Little_Endian);

	int getEFLAGS();


	ThreadContext ctx = {};
private:

	BYTE readByte(DWORD page,WORD offset);
	void writeByte(DWORD page, WORD offset, BYTE byte);


	BYTE** memPages = nullptr;
};

struct PageInfo {
	DWORD pageNum = 0;
	BYTE data[0x1000];
};

template<typename T>
inline StepStatus MemoryManager::readMem(DWORD addr, T& data, ByteDirection BDir) {
	int size = sizeof(T);

	// Increase read speed if 1 byte
	if (size == 1) {
		data = (T)readByte(addr / 0x1000, addr % 0x1000);
		return EM_OK;
	}

	if ((__int64)addr + (__int64)size > 0xFFFF'FFFF)
		return EM_INVALID_ADDRESS;

	if (BDir == BDir_Little_Endian) {
		BYTE* dataPtr = (BYTE*)&data + size - 1;
		for (__int64 page = (addr + size) / 0x1000; page >= 0; page--) {
			for (int offset = ((addr + size - 1) % 0x1000); offset >= 0 && size > 0; offset--, size--, dataPtr--) {
				*dataPtr = readByte(page, offset);
			}
		}
	}
	else if (BDir == BDir_Big_Endian) {
		BYTE* dataPtr = (BYTE*)&data;
		for (int page = ((__int64)addr) / 0x1000; page <= 0xFFFF'F; page++) {
			for (int offset = (addr % 0x1000); offset < 0x1000 && size > 0; offset++, dataPtr++, size--){
				*dataPtr = readByte(page, offset);
			}
		}
	}

	return EM_OK;
}

template<typename T>
inline StepStatus MemoryManager::writeMem(DWORD addr, T data, ByteDirection BDir) {
	int size = sizeof(T);

	// Increase write speed if 1 byte
	if (size == 1) {
		writeByte(addr / 0x1000, addr % 0x1000, data);
		return EM_OK;
	}

	if ((__int64)addr + (__int64)size > 0xFFFF'FFFF)
		return EM_INVALID_ADDRESS;

	if (BDir == BDir_Little_Endian) {
		BYTE* dataPtr = (BYTE*)&data + size - 1;
		for (__int64 page = (addr + size) / 0x1000; page >= 0; page--)
			for (int offset = (addr + size % 0x1000); offset >= 0 && size > 0; offset--, size--, dataPtr--)
				writeByte(page, offset, *dataPtr);
	}
	else if (BDir == BDir_Big_Endian) {
		BYTE* dataPtr = (BYTE*)&data;
		for (int page = ((__int64)addr) / 0x1000; page <= 0xFFFF'F; page++)
			for (int offset = (addr % 0x1000); offset < 0x1000 && size > 0; offset++, dataPtr++, size--)
				writeByte(page, offset, *dataPtr);
	}
	else 
		return EM_INVALID_ADDRESS;
	return EM_OK;
}
