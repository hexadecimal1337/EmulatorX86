#include "SettingManager.h"
#include <cstring>
#include <fstream>

void SettingsManager::load() {
	std::fstream file("Settings.bin", std::ios::in | std::ios::binary);
	if (file.is_open()) {
		file.seekg(0, std::ios::end);
		std::streamoff fileSize = file.tellg();
		file.seekg(0, std::ios::beg);
		if (fileSize == sizeof(Settings))
			file.read((char*)&set, sizeof(Settings));
		else {
			set = Settings();
			file.close();
			save();
			oldSet = set;
			return;
		}
		file.close();
	}
	oldSet = set;
}

void SettingsManager::save() {
	std::fstream file("Settings.bin", std::ios::out | std::ios::binary);
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
