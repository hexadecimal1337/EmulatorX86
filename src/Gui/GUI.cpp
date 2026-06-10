#include <windows.h>
#include <algorithm>
#include <sstream>
#include "GUI.h"
#include "Extern/ImGui/imgui_internal.h"
#include "Extern/ImGui/imgui_memory_editor.h"
#include "Logic/ProgramConsoleManager.h"
#include "Resources/ImGuiSettings.h"
#include <fstream>
namespace {
constexpr float DEFAULT_FONT_SIZE = 15.0f;

float fontScale(float size, float baseSize = DEFAULT_FONT_SIZE) {
	return (std::max)(0.6f, size / baseSize);
}

ImVec4 colorFromSetting(uint32_t color) {
	return ImGui::ColorConvertU32ToFloat4((ImU32)color);
}

void colorSettingEdit(const char* label, uint32_t& color) {
	ImVec4 value = colorFromSetting(color);
	if (ImGui::ColorEdit4(label, (float*)&value, ImGuiColorEditFlags_NoInputs))
		color = ImGui::ColorConvertFloat4ToU32(value);
}

void applyPurpleStyle() {
	ImGui::StyleColorsDark();
	ImGuiStyle& styles = ImGui::GetStyle();
	auto& colors = styles.Colors;
	colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.1f, 0.13f, 1.0f };
	colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_Border] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.29f };
	colors[ImGuiCol_BorderShadow] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.24f };
	colors[ImGuiCol_Text] = ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f };
	colors[ImGuiCol_TextDisabled] = ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f };
	colors[ImGuiCol_Header] = ImVec4{ 0.13f, 0.13f, 0.17f, 1.0f };
	colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
	colors[ImGuiCol_HeaderActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_Button] = ImVec4{ 0.13f, 0.13f, 0.17f, 1.0f };
	colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
	colors[ImGuiCol_ButtonActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_CheckMark] = ImVec4{ 0.74f, 0.58f, 0.98f, 1.0f };
	colors[ImGuiCol_PopupBg] = ImVec4{ 0.1f, 0.1f, 0.13f, 0.92f };
	colors[ImGuiCol_SliderGrab] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.54f };
	colors[ImGuiCol_SliderGrabActive] = ImVec4{ 0.74f, 0.58f, 0.98f, 0.54f };
	colors[ImGuiCol_FrameBg] = ImVec4{ 0.13f, 0.13f, 0.17f, 1.0f };
	colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
	colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_Tab] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_TabHovered] = ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f };
	colors[ImGuiCol_TabActive] = ImVec4{ 0.2f, 0.22f, 0.27f, 1.0f };
	colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_TitleBg] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_ScrollbarBg] = ImVec4{ 0.1f, 0.1f, 0.13f, 1.0f };
	colors[ImGuiCol_ScrollbarGrab] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4{ 0.19f, 0.2f, 0.25f, 1.0f };
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f };
	colors[ImGuiCol_Separator] = ImVec4{ 0.44f, 0.37f, 0.61f, 1.0f };
	colors[ImGuiCol_SeparatorHovered] = ImVec4{ 0.74f, 0.58f, 0.98f, 1.0f };
	colors[ImGuiCol_SeparatorActive] = ImVec4{ 0.84f, 0.58f, 1.0f, 1.0f };
	colors[ImGuiCol_ResizeGrip] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.29f };
	colors[ImGuiCol_ResizeGripHovered] = ImVec4{ 0.74f, 0.58f, 0.98f, 0.29f };
	colors[ImGuiCol_ResizeGripActive] = ImVec4{ 0.84f, 0.58f, 1.0f, 0.29f };
	colors[ImGuiCol_DockingPreview] = ImVec4{ 0.44f, 0.37f, 0.61f, 1.0f };
	styles.TabRounding = 8.0f;
	styles.ScrollbarRounding = 9.0f;
	styles.WindowRounding = 10.0f;
	styles.GrabRounding = 12.0f;
	styles.FrameRounding = 3.0f;
	styles.PopupRounding = 10.0f;
	styles.ChildRounding = 0.0f;
	styles.WindowTitleAlign.x = 0.5f;
	styles.SeparatorTextAlign.x = 0.5f;
}
void applyHighContrastStyle() {
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	auto& colors = style.Colors;
	colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
	colors[ImGuiCol_Border] = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
	colors[ImGuiCol_Button] = ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.25f, 0.0f, 1.0f);
	colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.0f, 1.0f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.0f, 1.0f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.4f, 0.4f, 0.0f, 1.0f);
	colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.03f, 0.03f, 0.03f, 1.0f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.0f, 1.0f);
	colors[ImGuiCol_Tab] = ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
	colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.18f, 0.0f, 1.0f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.3f, 0.3f, 0.0f, 1.0f);
}
}
void GUI::loadSettings() {
	sm.load();
	memoryWindows.setSize(sm.set.memEditorsNum);
	applyAppearanceSettings();
	const char* test = ImGui::SaveIniSettingsToMemory();
	std::ifstream fl("imgui.ini");
	if (!fl.is_open())
		ImGui::LoadIniSettingsFromMemory((const char*)Resource::ImGuiDefaultSettings,sizeof(Resource::ImGuiDefaultSettings));
	else
		fl.close();
}

void GUI::resetSettings() {
	sm.resetSettings();
	ImGui::LoadIniSettingsFromMemory((const char*)Resource::ImGuiDefaultSettings, sizeof(Resource::ImGuiDefaultSettings));
	memoryWindows.setSize(sm.set.memEditorsNum);
	applyAppearanceSettings();
}

void GUI::render() {
	GraphicsManager& gm = GraphicsManager::getGraphicsManager();
	if (isOpen) {
		applyAppearanceSettings();
		mainWindow();
		if(sm.set.isProcessor)
			controlWindow();
		settingsWindow();
		modal.renderModal();
		memoryWindows.renderEditors();
		assemblyWindow();
		assembler.autoSaveIfNeeded(sm.set);
		StepStatus status = em.getLastStatus();
		if(status < 0) {
			em.resetStatus();
			modal.openModal((const char*)u8"Ошибка выполнения", em.getStatusStr(status));
		}
		sm.set.memEditorsNum = memoryWindows.getSize();
		sm.syncSettings();
	}
	else
		SendMessageA(gm.getHWND(), WM_DESTROY, 0, 0);
}

void GUI::mainWindow() {
	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	style.WindowMenuButtonPosition = ImGuiDir_Right;
	ImGui::Begin((const char*)u8"Эмулятор x86", &isOpen, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar);
	style.WindowMenuButtonPosition = ImGuiDir_Left;
	ImGui::PopStyleVar();
	ImGuiID dockspaceID = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspaceID, ImVec2(0.f, 0.f));
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu((const char*)u8"Эмулятор")) {
			if (ImGui::MenuItem((const char*)u8"Сохранить состояние")) {
				if (!em.isWorking())
					saveDmpFile();
				else
					modal.openModal((const char*)u8"Ошибка сохранения состояния", (const char*)u8"Невозможно сохранить состояние\nОстановите программу");
			}
			if (ImGui::MenuItem((const char*)u8"Загрузить состояние")) {
				if (!em.isWorking())
					loadDmpFile();
				else
					modal.openModal((const char*)u8"Ошибка загрузки состояния", (const char*)u8"Невозможно загрузить состояние\nОстановите программу");
			}
			if(ImGui::MenuItem((const char*)u8"Сбросить состояние")) {
				if (!em.isWorking())
					mm.reset();
				else
					modal.openModal((const char*)u8"Ошибка сброса состояния", (const char*)u8"Невозможно сбросить состояние\nОстановите программу");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu((const char*)u8"Вид")) {
			if (ImGui::MenuItem((const char*)u8"Процессор"))
				sm.set.isProcessor = true;
			if (ImGui::MenuItem((const char*)u8"Редактор памяти"))
				memoryWindows.openNewEditor();
			if (ImGui::MenuItem((const char*)u8"Ассемблер"))
				sm.set.isAssembler = true;
			//if (ImGui::MenuItem((const char*)u8"Дизассемблер"))
			//	sm.set.isDisassembler = true;
			if (ImGui::MenuItem((const char*)u8"Консоль программы"))
				sm.set.isConsole = true;
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem((const char*)u8"Настройки"))
			isSettingsOpen = true;
		ImGui::MenuItem((const char*)u8"Справка");
		ImGui::EndMenuBar();
	}
	ImGui::End();
	//if (sm.set.isDisassembler) {
	//	ImGui::Begin((const char*)u8"Дизассемблер",&sm.set.isDisassembler);
	//	ImGui::End();
	//}
	if (sm.set.isConsole) {
		programConsole.render(&sm.set.isConsole, sm.set);
	}
}

void GUI::controlWindow() {
	ImGui::Begin((const char*)u8"Процессор", &sm.set.isProcessor);
	std::lock_guard<std::recursive_mutex> cpuLock(mm.getStateMutex());

	static int test = 0;
	int EFLAGS = mm.getEFLAGS();
	int frequency = mm.ctx.frequency;

	ImGui::SeparatorText((const char*)u8"Управление");
	if(em.isWorking())
		ImGui::Text((const char*)u8"Состояние: В процессе");
	else
		ImGui::Text((const char*)u8"Состояние: Приостановлено");
		
	if (ImGui::Button((const char*)u8"Шаг"))
		em.makeStepIn();
	if (ImGui::Button((const char*)u8"Шаг с обходом")) {
		//em.makeStepDetour();
	}
	if (ImGui::Button((const char*)u8"Шаг с выходом")) {
		//em.makeStepOut();
	}
	if (ImGui::Button((const char*)u8"Выполнить")) {
		em.run();
	}
	if (ImGui::Button((const char*)u8"Остановить")) {
		em.stop();
	}
	ImGui::SeparatorText((const char*)u8"Регистры");
	ImGui::DragInt("EAX", (int*)&mm.ctx.EAX, 0, 0, 0, "%08X");
	ImGui::DragInt("EBX", (int*)&mm.ctx.EBX, 0, 0, 0, "%08X");
	ImGui::DragInt("ECX", (int*)&mm.ctx.ECX, 0, 0, 0, "%08X");
	ImGui::DragInt("EDX", (int*)&mm.ctx.EDX, 0, 0, 0, "%08X");
	ImGui::DragInt("ESP", (int*)&mm.ctx.ESP, 0, 0, 0, "%08X");
	ImGui::DragInt("EBP", (int*)&mm.ctx.EBP, 0, 0, 0, "%08X");
	ImGui::DragInt("ESI", (int*)&mm.ctx.ESI, 0, 0, 0, "%08X");
	ImGui::DragInt("EDI", (int*)&mm.ctx.EDI, 0, 0, 0, "%08X");
	ImGui::DragInt("EIP", (int*)&mm.ctx.EIP, 0, 0, 0, "%08X");
	ImGui::SeparatorText((const char*)u8"Регистр флагов");
	ImGui::DragInt("EFLAGS", &EFLAGS, 0, 0, 0, "%08X");
	ImGui::PushItemWidth(20);
	drawFLAG("CF", EFLAG_CF);
	drawFLAG("PF", EFLAG_PF);
	ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 60, ImGui::GetCursorPosY() - 50 });
	drawFLAG("AF", EFLAG_AF);
	ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 60, ImGui::GetCursorPosY() });
	drawFLAG("ZF", EFLAG_ZF);
	ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 120, ImGui::GetCursorPosY() - 50 });
	drawFLAG("SF", EFLAG_SF);
	ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 120, ImGui::GetCursorPosY() });
	drawFLAG("OF", EFLAG_OF);
	ImGui::SeparatorText((const char*)u8"Частота и счётчик тактов");
	ImGui::PushItemWidth(70);
	ImGui::DragInt((const char*)u8"Гц    Частота процессора", &frequency, 0, 0, 0,"%d");
	unsigned long long counter = mm.ctx.counter;
	ImGui::DragScalar((const char*)u8"        Счётчик тактов", ImGuiDataType_U64, &counter, 0, nullptr, nullptr, "%llu");
	mm.ctx.counter = counter;
	if (frequency > 0 && frequency <= 1'000'000)
		mm.ctx.frequency = frequency;
	ImGui::End();
}

void GUI::drawFLAG(const char* flagName, int flagID) {
	char str[2] = "0";
	str[0] = mm.ctx.EFLAGS[flagID] + 0x30;
	ImGui::PushID(flagName);
	if (ImGui::Button(str, { 0,0 }))
		mm.ctx.EFLAGS[flagID] = !mm.ctx.EFLAGS[flagID];
	ImGui::SameLine();
	ImGui::Text(flagName);
	ImGui::PopID();
}

void GUI::applyAppearanceSettings() {
	sm.set.uiFontSize = std::clamp(sm.set.uiFontSize, 12.0f, 24.0f);
	sm.set.editorFontSize = std::clamp(sm.set.editorFontSize, 12.0f, 28.0f);
	sm.set.consoleFontSize = std::clamp(sm.set.consoleFontSize, 12.0f, 28.0f);
	sm.set.editorTabSize = std::clamp(sm.set.editorTabSize, 1, 8);
	ImGui::GetIO().FontGlobalScale = fontScale(sm.set.uiFontSize);
	switch (sm.set.appTheme) {
	case APP_THEME_LIGHT:
		ImGui::StyleColorsLight();
		break;
	case APP_THEME_BLUE:
		ImGui::StyleColorsClassic();
		break;
	case APP_THEME_HIGH_CONTRAST:
		applyHighContrastStyle();
		break;
	case APP_THEME_DARK:
		ImGui::StyleColorsDark();
		break;
	case APP_THEME_PURPLE:
	default:
		applyPurpleStyle();
		break;
	}
	if (sm.set.appTheme != APP_THEME_PURPLE) {
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 7.0f;
		style.ChildRounding = 4.0f;
		style.FrameRounding = 3.0f;
		style.PopupRounding = 4.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 4.0f;
		style.WindowTitleAlign.x = 0.5f;
		style.SeparatorTextAlign.x = 0.5f;
	}
}
void GUI::settingsWindow() {
	ImGui::SetNextWindowSize({ 520,640 }, ImGuiCond_FirstUseEver);
	if (isSettingsOpen) {
		ImGui::Begin((const char*)u8"Настройки", &isSettingsOpen);
		if (ImGui::Button((const char*)u8"Сбросить настройки"))
			resetSettings();
		ImGui::Separator();
		if (ImGui::BeginTabBar("SettingsTabs")) {
			if (ImGui::BeginTabItem((const char*)u8"Внешний вид")) {
				drawAppearanceSettings();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem((const char*)u8"Консоль")) {
				drawConsoleSettings();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem((const char*)u8"Проект")) {
				drawProjectSettings();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}
}

void GUI::drawAppearanceSettings() {
	const char* themes[] = { (const char*)u8"Фиолетовая", (const char*)u8"Светлая", (const char*)u8"IDE Blue", (const char*)u8"Высокий контраст", (const char*)u8"Тёмная" };
	ImGui::Combo((const char*)u8"Тема интерфейса", &sm.set.appTheme, themes, IM_ARRAYSIZE(themes));
	ImGui::SliderFloat((const char*)u8"Шрифт интерфейса", &sm.set.uiFontSize, 12.0f, 24.0f, "%.0f px");
	ImGui::SliderFloat((const char*)u8"Шрифт редактора", &sm.set.editorFontSize, 12.0f, 28.0f, "%.0f px");
	ImGui::SeparatorText((const char*)u8"Редактор");
	ImGui::Checkbox((const char*)u8"Номера строк", &sm.set.editorShowLineNumbers);
	ImGui::Checkbox((const char*)u8"Подсветка текущей строки", &sm.set.editorHighlightCurrentLine);
	ImGui::Checkbox((const char*)u8"Показывать пробелы и табы", &sm.set.editorShowWhitespaces);
	ImGui::Checkbox((const char*)u8"Автоотступ", &sm.set.editorAutoIndent);
	ImGui::SliderInt((const char*)u8"Размер табуляции", &sm.set.editorTabSize, 1, 8);
	ImGui::SeparatorText((const char*)u8"Палитра ASM");
	colorSettingEdit((const char*)u8"Команды", sm.set.asmInstructionColor);
	colorSettingEdit((const char*)u8"Регистры", sm.set.asmRegisterColor);
	colorSettingEdit((const char*)u8"Числа", sm.set.asmNumberColor);
	colorSettingEdit((const char*)u8"Строки", sm.set.asmStringColor);
	colorSettingEdit((const char*)u8"Комментарии", sm.set.asmCommentColor);
	colorSettingEdit((const char*)u8"Метки", sm.set.asmLabelColor);
	colorSettingEdit((const char*)u8"Ошибки компиляции", sm.set.asmErrorColor);
}

void GUI::drawConsoleSettings() {
	ImGui::SliderFloat((const char*)u8"Шрифт консоли", &sm.set.consoleFontSize, 12.0f, 28.0f, "%.0f px");
	colorSettingEdit((const char*)u8"Цвет текста", sm.set.consoleTextColor);
	colorSettingEdit((const char*)u8"Цвет фона", sm.set.consoleBgColor);
}

void GUI::drawProjectSettings() {
	ImGui::Checkbox((const char*)u8"Автосохранение кода в Autosave.asm", &sm.set.projectAutosaveCode);
}
void GUI::assemblyWindow() {
	if (sm.set.isAssembler) {
		ImGui::Begin((const char*)u8"Ассемблер", &sm.set.isAssembler,ImGuiWindowFlags_MenuBar);
		ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
		if (ImGui::BeginMenuBar()) {
			if (ImGui::MenuItem((const char*)u8"Скомпилировать")) {
				if (!em.isWorking()) {
					StepStatus status = assembler.compile();
					if(status < 0)
						modal.openModal((const char*)u8"Ошибка компиляции", (const char*)u8"Проверьте строки с ошибками");
				}
				else
					modal.openModal((const char*)u8"Ошибка компиляции", (const char*)u8"Остановите программу");
			}
			if (ImGui::MenuItem((const char*)u8"Сохранить")) {
				if(!assembler.saveFileAs())
					modal.openModal((const char*)"Ошибка сохранения файла", (const char*)u8"Не удалось сохранить asm файл");
			}
			if (ImGui::MenuItem((const char*)u8"Загрузить")) {
				if (!assembler.loadFileAs())
					modal.openModal((const char*)"Ошибка загрузки файла", (const char*)u8"Не удалось загрузить asm файл");
			}
			ImGui::EndMenuBar();
		}
		assembler.render("TextEditor", sm.set);
		ImGui::End();
	}
}

bool GUI::loadDmpFile() {
	OPENFILENAMEW open = {};
	wchar_t filePath[MAX_PATH];
	filePath[0] = L'\0';
	open.lStructSize = sizeof(OPENFILENAMEA);
	open.lpstrFilter = L"Дамп памяти (*.dmp)\0*.dmp\0\0";
	open.lpstrFile = filePath;
	open.nMaxFile = MAX_PATH;
	open.lpstrTitle = L"Выберите dmp файл для чтения";
	open.Flags = OFN_FILEMUSTEXIST;
	if (GetOpenFileNameW(&open)) {
		if (mm.loadFromFile(filePath))
			return true;
		else {
			modal.openModal((const char*)"Ошибка загрузки состояния", (const char*)u8"Не удалось загрузить DMP файл");
			return true;
		}
	}
	else
		return false;
}

bool GUI::saveDmpFile() {
	OPENFILENAMEW open = {};
	wchar_t filePath[MAX_PATH];
	filePath[0] = L'\0';
	open.lStructSize = sizeof(OPENFILENAMEA);
	open.lpstrFilter = L"Дамп памяти (*.dmp)\0*.dmp\0\0";
	open.lpstrDefExt = L"dmp";
	open.lpstrFile = filePath;
	open.nMaxFile = MAX_PATH;
	open.lpstrTitle = L"Выберите путь для сохранения dmp файла";
	open.Flags = OFN_FILEMUSTEXIST | OFN_OVERWRITEPROMPT;
	if (GetSaveFileNameW(&open)) {
		if (mm.saveToFile(filePath))
			return true;
		else {
			modal.openModal((const char*)"Ошибка сохранения состояния", (const char*)u8"Не удалось сохранить DMP файл");
			return true;
		}
	}
	else
		return false;
}

void ModalWindow::renderModal() {
	bool open = true;
	if (isModalPopped) {
		ImGui::OpenPopup(header.c_str());
		isModalPopped = false;
	}
	static ImVec2 screenCenter = { (float)GetSystemMetrics(SM_CXFULLSCREEN) / 2 - 280, (float)GetSystemMetrics(SM_CYFULLSCREEN) / 2 - 225 };
	ImGui::SetNextWindowPos(screenCenter);
	ImGui::SetNextWindowSize({ 280,225 });
	if (ImGui::BeginPopupModal(header.c_str(), &open, ImGuiWindowFlags_NoResize)) {
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
		ImGui::SetCursorPos({ (windowSize.x - textSize.x) * 0.5f ,(windowSize.y - textSize.y) * 0.5f - 10 });
		ImGui::Text(text.c_str());
		ImGui::SetCursorPos({ 90, 195 });
		if (ImGui::Button((const char*)u8"Продолжить"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

void ModalWindow::openModal(const std::string header,const std::string msg) {
	this->header = header;
	text = msg;
	isModalPopped = true;
}

bool MemoryWindows::openNewEditor() {
	if (editors.size() < 9)
		editors.push_back(new MemoryEditor());
	else
		return false;
	return true;
}

void MemoryWindows::renderEditors() {
	int i = 0;
	for (auto it = editors.begin(); it != editors.end(); ++i) {
		char8_t label[32] = u8"Редактор памяти  ";
		if(i != 0)
			label[30] = i + 1 + 0x30;
		MemoryEditor* editor = (MemoryEditor*)*it;
		if (editor->Open) {
			editor->DrawWindow((const char*)label, 0, 0x8000'0000);
			++it;
		}
		else
			editors.erase(it++);
	}
}

void MemoryWindows::setSize(int newSize) {
	editors.clear();
	while(editors.size() < 9 && editors.size() < newSize)
		editors.push_back(new MemoryEditor());
}

void ProgramConsoleWindow::render(bool* open, const Settings& settings) {
    ProgramConsoleManager& console = ProgramConsoleManager::getProgramConsoleManager();
    ImGui::Begin((const char*)u8"Консоль программы", open);
    ImGui::SetWindowFontScale(fontScale(settings.consoleFontSize, settings.uiFontSize));

    if (ImGui::Button((const char*)u8"Очистить"))
        console.clear();

    ImGui::Separator();
    float inputHeight = ImGui::GetFrameHeightWithSpacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, colorFromSetting(settings.consoleBgColor));
    ImGui::PushStyleColor(ImGuiCol_Text, colorFromSetting(settings.consoleTextColor));
    ImGui::BeginChild("ProgramConsoleOutput", ImVec2(0, -inputHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
    std::string output = console.getOutput();
    ImGui::TextUnformatted(output.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    bool isWaitingInput = console.isInputWaiting();
    ImGui::PushItemWidth(-1);
    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;
    if (!isWaitingInput)
        inputFlags |= ImGuiInputTextFlags_ReadOnly;
    if (ImGui::InputText("##ProgramConsoleInput", inputBuffer, sizeof(inputBuffer), inputFlags)) {
        if (isWaitingInput)
            console.pushInput(inputBuffer);
        inputBuffer[0] = '\0';
    }
    ImGui::PopItemWidth();

    ImGui::SetWindowFontScale(1.0f);
    ImGui::End();
}
Assembler::Assembler() {
	auto lang = getAsmLanguage();
	textEditor.SetLanguageDefinition(lang);
	textEditor.SetShowWhitespaces(false);
}

void Assembler::render(const char* title, const Settings& settings) {
	applySettings(settings);
	ImGui::SetWindowFontScale(fontScale(settings.editorFontSize, settings.uiFontSize));
	textEditor.Render(title);
	ImGui::SetWindowFontScale(1.0f);
}

void Assembler::applySettings(const Settings& settings) {
	TextEditor::Palette palette = settings.appTheme == APP_THEME_LIGHT ? TextEditor::GetLightPalette() : TextEditor::GetDarkPalette();
	if (settings.appTheme == APP_THEME_BLUE)
		palette = TextEditor::GetRetroBluePalette();
	palette[(int)TextEditor::PaletteIndex::KnownIdentifier] = settings.asmInstructionColor;
	palette[(int)TextEditor::PaletteIndex::Keyword] = settings.asmRegisterColor;
	palette[(int)TextEditor::PaletteIndex::Number] = settings.asmNumberColor;
	palette[(int)TextEditor::PaletteIndex::String] = settings.asmStringColor;
	palette[(int)TextEditor::PaletteIndex::Comment] = settings.asmCommentColor;
	palette[(int)TextEditor::PaletteIndex::Identifier] = settings.asmLabelColor;
	palette[(int)TextEditor::PaletteIndex::ErrorMarker] = settings.asmErrorColor;
	textEditor.SetPalette(palette);
	textEditor.SetShowLineNumbers(settings.editorShowLineNumbers);
	textEditor.SetHighlightCurrentLine(settings.editorHighlightCurrentLine);
	textEditor.SetShowWhitespaces(settings.editorShowWhitespaces);
	textEditor.SetTabSize(settings.editorTabSize);
	TextEditor::LanguageDefinition lang = textEditor.GetLanguageDefinition();
	lang.mAutoIndentation = settings.editorAutoIndent;
	textEditor.SetLanguageDefinition(lang);
}

void Assembler::autoSaveIfNeeded(const Settings& settings) {
	if (!settings.projectAutosaveCode) {
		lastAutoSaveText.clear();
		return;
	}
	std::string text = textEditor.GetText();
	if (text == lastAutoSaveText)
		return;
	std::fstream file("Autosave.asm", std::ios::out | std::ios::binary);
	if (!file.is_open())
		return;
	file.write(text.c_str(), text.size());
	file.close();
	lastAutoSaveText = text;
}
StepStatus Assembler::compile() {
	std::lock_guard<std::recursive_mutex> cpuLock(mm.getStateMutex());
	std::vector<std::string> lines = textEditor.GetTextLines();
	std::vector<BYTE> code;
	std::vector<std::pair<int, StepStatus>> errors;
	StepStatus result = im.parseCode(lines, code, errors);
	if (result == EM_OK) {
		for (int i = 0; i < code.size(); ++i)
			mm.writeMem(mm.ctx.EIP + i, code[i]);
	}
	TextEditor::ErrorMarkers markers;
	for (const auto& error : errors)
		markers.insert(std::pair<int, std::string>(error.first, getErrorMsg(error.second)));
	textEditor.SetErrorMarkers(markers);
	return result;
}

bool Assembler::saveFileAs() {
	OPENFILENAMEW open = {};
	wchar_t filePath[MAX_PATH];
	filePath[0] = L'\0';
	open.lStructSize = sizeof(OPENFILENAMEA);
	open.lpstrFilter = L"Код на языке ассемблера (*.asm)\0*.asm\0\0";
	open.lpstrDefExt = L"asm";
	open.lpstrFile = filePath;
	open.nMaxFile = MAX_PATH;
	open.lpstrTitle = L"Выберите путь для сохранения asm файла";
	open.Flags = OFN_FILEMUSTEXIST | OFN_OVERWRITEPROMPT;
	if (GetSaveFileNameW(&open)) {
		if (saveFile(filePath))
			return true;
		else
			return false;
	}
	else
		return true;
}

bool Assembler::saveFile(const wchar_t* filePath) {
	std::fstream file(filePath, std::ios::out);
	if (!file.is_open())
		return false;
	std::string text = textEditor.GetText();
	file.write(text.c_str(), text.size());
	file.close();
	return true;
}

bool Assembler::loadFileAs() {
	OPENFILENAMEW open = {};
	wchar_t filePath[MAX_PATH];
	filePath[0] = L'\0';
	open.lStructSize = sizeof(OPENFILENAMEA);
	open.lpstrFilter = L"Код на языке ассемблера (*.asm)\0*.asm\0\0";
	open.lpstrFile = filePath;
	open.nMaxFile = MAX_PATH;
	open.lpstrTitle = L"Выберите asm файл для чтения";
	open.Flags = OFN_FILEMUSTEXIST;
	if (GetOpenFileNameW(&open)) {
		if (loadFile(filePath))
			return true;
		else
			return false;
	}
	else
		return true;
}

bool Assembler::loadFile(const wchar_t* filePath) {
	std::fstream file(filePath, std::ios::in);
	if (!file.is_open())
		return false;
	std::stringstream strStream;
	strStream << file.rdbuf();
	textEditor.SetText(strStream.str());
	file.close();
	return true;
}

TextEditor::LanguageDefinition Assembler::getAsmLanguage() {
	TextEditor::LanguageDefinition lang = textEditor.GetLanguageDefinition();
	lang.mName = "ASM";
	lang.mSingleLineComment = "//";
	lang.mCaseSensitive = true;

	using PaletteIndex = TextEditor::PaletteIndex;
	lang.mTokenRegexStrings.insert(lang.mTokenRegexStrings.begin(), std::make_pair<std::string, PaletteIndex>("[0-9a-fA-F]+[hH]", PaletteIndex::Number));

	static const char* const identifiers[] = {
		"nop", "or", "and", "xor", "cmp", "jmp", "jo", "jno", "jb", "jnb", "je", "jne", "jna", "jbe", "ja", "js", "jns", "jl", "jge", "jle", "jg", "sub", "add", "inc", "dec", "not", "neg", "shl", "shr", "sar", "mul", "imul", "div", "idiv", "lea", "xchg", "call", "ret", "clc", "stc", "test", "mov", "int3", "push", "pop", "syscall", "db", "dd", "offset"
	};
	for (auto& k : identifiers) {
		TextEditor::Identifier id;
		id.mDeclaration = "Instructions";
		lang.mIdentifiers.insert(std::make_pair(std::string(k), id));
	}

	lang.mKeywords.clear();
	static const char* const keywords[] = { "eax", "ebx", "ecx", "edx", "ebp", "esp", "esi", "edi", "eip" };
	for (auto keyword : keywords)
		lang.mKeywords.insert(std::string(keyword));
	return lang;
}
std::string Assembler::getErrorMsg(StepStatus error) {
	std::string result;
	switch (error) {
		case EM_INVALID_INSTRUCTION:
			result = (const char*)u8"Неизвестная инструкция:\nНеверный опкод\nПроверьте код команды";
			break;
		case EM_INVALID_OPERAND:
			result = (const char*)u8"Ошибка операнда:\nОдин из операндов неверен\nПроверьте операнды/инструкцию";
			break;
		case EM_INVALID_ADDRESS:
			result = (const char*)u8"Ошибка доступа:\nПопытка чтения/записи/выполнения в недопустимой памяти\nПроверьте EIP/операнды/инструкцию";
			break;
		case EM_INVALID_SYNTAX:
			result = (const char*)u8"Ошибка синтаксиса:\nНеизвестный синтаксис\nПроверьте правильность написания инструкции";
			break;
		default:
			result = (const char*)u8"Неизвестная ошибка";
	}
	return result;
}
