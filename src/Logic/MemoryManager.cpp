#include "MemoryManager.h"
#include <list>
#include <fstream>

void MemoryManager::init() {
	std::lock_guard<std::recursive_mutex> lock(stateMutex);
	if (memPages)
		return;
	memPages = new BYTE* [PAGE_COUNT];
	for (int i = 0; i < PAGE_COUNT; ++i)
		memPages[i] = nullptr;
	ctx = ThreadContext();
	ctx.EBP = 0x7FFF'0000;
	ctx.ESP = 0x7FFF'0000;
	ctx.EIP = 0x0040'1000;
	ctx.EFLAGS[1] = 1;
	ctx.EFLAGS[21] = 1;
	ctx.frequency = 1'000'000;
}

void MemoryManager::destroy() {
	std::lock_guard<std::recursive_mutex> lock(stateMutex);
	if (memPages == nullptr)
		return;
	for (int i = 0; i < PAGE_COUNT; ++i)
		delete[] memPages[i];
	delete[] memPages;
	memPages = nullptr;
}

void MemoryManager::optimize() {
	std::lock_guard<std::recursive_mutex> lock(stateMutex);
	for (int i = 0; i < PAGE_COUNT; ++i) {
		if (memPages[i]) {
			bool isEmpty = true;
			for (int j = 0; j < 0x1'000 && isEmpty;++j)
				isEmpty = memPages[i][j] == 0;
			if (isEmpty) {
				delete[] memPages[i];
				memPages[i] = nullptr;
			}
		}
	}
}

bool MemoryManager::saveToFile(const wchar_t* filePath) {
	std::lock_guard<std::recursive_mutex> lock(stateMutex);
	std::list<PageInfo> pagesInfo;
	std::fstream file(filePath,std::ios::out);
	if (!file.is_open())
		return false;
	for (int i = 0; i < PAGE_COUNT; ++i) {
		if (memPages[i]) {
			bool isEmpty = true;
			for (int j = 0; j < 0x1'000 && isEmpty; ++j)
				isEmpty = memPages[i][j] == 0;
			if (!isEmpty) {
				PageInfo page = {};
				page.pageNum = i;
				for (int j = 0; j < 0x1'000; ++j)
					page.data[j] = memPages[i][j];
				pagesInfo.emplace_back(page);
			}
		}
	}
	file.write("MEMORYDUMP", 10);
	file << pagesInfo.size();
	for (const PageInfo& page : pagesInfo)
		file.write((const char*)&page, sizeof(page));
	file.write((const char*)&ctx, sizeof(ctx));
	file.close();
	return true;
}

bool MemoryManager::loadFromFile(const wchar_t* filePath) {
	std::lock_guard<std::recursive_mutex> lock(stateMutex);
	std::fstream file(filePath, std::ios::in);
	if (!file.is_open())
		return false;
	char extension[11];
	extension[10] = '\0';
	file.read(extension, 10);
	if (strcmp(extension, "MEMORYDUMP") != 0)
		return false;
	reset();
	size_t size = 0;
	file >> size;
	PageInfo* pagesInfo = new PageInfo[size];
	file.read((char*)pagesInfo,size * sizeof(PageInfo));
	file.read((char*)&ctx, sizeof(ctx));
	for (int i = 0; i < size; ++i) {
		PageInfo* page = pagesInfo + i;
		if (page->pageNum >= PAGE_COUNT)
			continue;
		if (memPages[page->pageNum] == nullptr)
			memPages[page->pageNum] = new BYTE[0x1000];
		memcpy(memPages[page->pageNum],page->data,0x1000);
	}
	delete[] pagesInfo;
	file.close();
	return true;
}

void MemoryManager::reset() {
	std::lock_guard<std::recursive_mutex> lock(stateMutex);
	destroy();
	init();
}

std::recursive_mutex& MemoryManager::getStateMutex() {
	return stateMutex;
}

int MemoryManager::getEFLAGS() {
	std::lock_guard<std::recursive_mutex> lock(stateMutex);
	int EFLAGS = 0;
	for (int i = 0; i < 32; ++i) {
		EFLAGS |= ctx.EFLAGS[i] << i;
	}
	return EFLAGS;
}

BYTE MemoryManager::readByte(DWORD page, WORD offset) {
	if (memPages == nullptr || page >= PAGE_COUNT || memPages[page] == nullptr)
		return 0;
	return memPages[page][offset];
}

void MemoryManager::writeByte(DWORD page, WORD offset, BYTE byte) {
	if (memPages == nullptr || page >= PAGE_COUNT)
		return;
	if (memPages[page] == nullptr)
		memPages[page] = new BYTE[0x1000]();
	memPages[page][offset] = byte;
}


