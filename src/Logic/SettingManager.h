#pragma once
#include <cstdint>

enum AppTheme {
	APP_THEME_PURPLE = 0,
	APP_THEME_LIGHT = 1,
	APP_THEME_BLUE = 2,
	APP_THEME_HIGH_CONTRAST = 3,
	APP_THEME_DARK = 4
};

struct Settings {
	int memEditorsNum = 2;
	bool isAssembler = true;
	bool isDisassembler = true;
	bool isConsole = true;
	bool isProcessor = true;
	int appTheme = APP_THEME_PURPLE;
	float uiFontSize = 15.0f;
	float editorFontSize = 15.0f;
	float consoleFontSize = 15.0f;
	bool editorShowLineNumbers = true;
	bool editorHighlightCurrentLine = true;
	bool editorShowWhitespaces = false;
	bool editorAutoIndent = true;
	int editorTabSize = 4;
	uint32_t asmInstructionColor = 0xffdcdcaa;
	uint32_t asmRegisterColor = 0xff9cdcfe;
	uint32_t asmNumberColor = 0xffb5cea8;
	uint32_t asmStringColor = 0xffce9178;
	uint32_t asmCommentColor = 0xff6a9955;
	uint32_t asmLabelColor = 0xff4ec9b0;
	uint32_t asmErrorColor = 0xff7a1f1f;
	uint32_t consoleTextColor = 0xfff2f2f2;
	uint32_t consoleBgColor = 0xff121212;
	bool projectAutosaveCode = false;
};

class SettingsManager {
public:
	inline static SettingsManager& getSettingsManager() {
		static SettingsManager settingsManager;
		return settingsManager;
	}

	void load();
	void save();
	void resetSettings();
	void syncSettings();

	Settings set;
private:
	SettingsManager() = default;

	Settings oldSet = set;
};
