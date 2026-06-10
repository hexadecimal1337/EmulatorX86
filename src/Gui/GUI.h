#pragma once
#include "Extern/ImGui/imgui.h"
#include "GraphicsManager.h"
#include "Logic/EmulatorManager.h"
#include "Logic/SettingManager.h"
#include "Extern/ImGui/TextEditor.h"
#include <list>
#include <string>

class ProgramConsoleWindow {
public:
	void render(bool* open, const Settings& settings);

private:
	char inputBuffer[1024] = {};
};

class Assembler {
public:
	Assembler();
	void render(const char* title, const Settings& settings);
	StepStatus compile();
	bool saveFileAs();
	bool saveFile(const wchar_t* filePath);
	bool loadFileAs();
	bool loadFile(const wchar_t* filePath);
	void autoSaveIfNeeded(const Settings& settings);
private:
	TextEditor::LanguageDefinition getAsmLanguage();
	std::string getErrorMsg(StepStatus error);
	void applySettings(const Settings& settings);

	TextEditor textEditor;
	std::string lastAutoSaveText;
	InstructionManager& im = InstructionManager::getInstructionManager();
	MemoryManager& mm = MemoryManager::getMemoryManager();
};

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
	void assemblyWindow();
	bool loadDmpFile();
	bool saveDmpFile();
	void applyAppearanceSettings();
	void drawAppearanceSettings();
	void drawConsoleSettings();
	void drawProjectSettings();

	bool isOpen = true;
	bool isSettingsOpen = false;
	GUI() = default;

	ModalWindow modal;
	MemoryWindows memoryWindows;
	ProgramConsoleWindow programConsole;
	Assembler assembler;

	MemoryManager& mm = MemoryManager::getMemoryManager();
	EmulatorManager& em = EmulatorManager::getEmulatorManager();
	SettingsManager& sm = SettingsManager::getSettingsManager();
};

