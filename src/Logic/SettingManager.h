#pragma once

struct Settings {
	int memEditorsNum = 2;
	bool isAssembler = true;
	bool isDisassembler = true;
	bool isConsole = true;
	bool isProcessor = true;
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