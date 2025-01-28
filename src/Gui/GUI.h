#pragma once
#include "Extern/ImGui/imgui.h"
#include "GraphicsManager.h"
#include "Logic/EmulatorManager.h"
#include "Logic/SettingManager.h"
#include <list>

class ModalWindow {
public:
	ModalWindow() = default;

	void renderModal();
	void openModal(const std::string header,const std::string msg);

private:
	std::string header = (const char*)u8"Модальное окно";
	std::string text = "";
	bool isModalPopped = false;
};

class MemoryWindows {
public:
	bool openNewEditor();
	void renderEditors();
	void setSize(int newSize);
	inline size_t getSize() { return editors.size();};
private:
	std::list<void*> editors;
};

class GUI {
public:
	inline static GUI& getGUI() {
		static GUI gui;
		return gui;
	}

	void loadSettings();
	void resetSettings();
	void render();

private:
	void mainWindow();
	void controlWindow();
	void drawFLAG(const char* flagName, int flagID);
	void settingsWindow();
	bool loadDmpFile();
	bool saveDmpFile();

	bool isOpen = true;
	bool isSettingsOpen = false;
	GUI() = default;

	ModalWindow modal;
	MemoryWindows memoryWindows;

	MemoryManager& mm = MemoryManager::getMemoryManager();
	EmulatorManager& em = EmulatorManager::getEmulatorManager();
	SettingsManager& sm = SettingsManager::getSettingsManager();
};

