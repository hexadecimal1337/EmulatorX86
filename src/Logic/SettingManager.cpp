#include "SettingManager.h"
#include <fstream>

void SettingsManager::load() {
	std::fstream file("Settings.bin", std::ios::in);
	if (file.is_open()) {
		file.read((char*)&set,sizeof(Settings));
		file.close();
	}
	oldSet = set;
}

void SettingsManager::save() {
	std::fstream file("Settings.bin",std::ios::out);
	if (file.is_open()) {
		file.write((char*)&set, sizeof(Settings));
		file.close();
	}
}

void SettingsManager::resetSettings() {
	set = Settings();
	oldSet = set;
	save();
}

void SettingsManager::syncSettings() {
	if (memcmp(&set, &oldSet, sizeof(Settings)) != 0) {
		oldSet = set;
		save();
	}
}
