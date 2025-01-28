#include <windows.h>
#include "GUI.h"
#include "Extern/ImGui/imgui_internal.h"
#include "Extern/ImGui/imgui_memory_editor.h"
#include "Resources/ImGuiSettings.h"
#include <fstream>

void GUI::loadSettings() {
	sm.load();
	memoryWindows.setSize(sm.set.memEditorsNum);
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
}

void GUI::render() {
	GraphicsManager& gm = GraphicsManager::getGraphicsManager();
	if (isOpen) {
		mainWindow();
		if(sm.set.isProcessor)
			controlWindow();
		settingsWindow();
		modal.renderModal();
		memoryWindows.renderEditors();
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
			if (ImGui::MenuItem((const char*)u8"Дизассемблер"))
				sm.set.isDisassembler = true;
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
	if (sm.set.isAssembler) {
		ImGui::Begin((const char*)u8"Ассемблер",&sm.set.isAssembler);
		ImGui::End();
	}
	if (sm.set.isDisassembler) {
		ImGui::Begin((const char*)u8"Дизассемблер",&sm.set.isDisassembler);
		ImGui::End();
	}
	if (sm.set.isConsole) {
		ImGui::Begin((const char*)u8"Консоль программы", &sm.set.isConsole);
		ImGui::End();
	}
}

void GUI::controlWindow() {
	ImGui::Begin((const char*)u8"Процессор", &sm.set.isProcessor);

	static int test = 0;
	int EFLAGS = mm.getEFLAGS();
	int frequency = mm.ctx.frequency;

	ImGui::SeparatorText((const char*)u8"Управление");
	if(em.isWorking())
		ImGui::Text((const char*)u8"Состояние: В процессе");
	else
		ImGui::Text((const char*)u8"Состояние: Приостановлено");
		
	if (ImGui::Button((const char*)u8"Шаг с заходом"))
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
	ImGui::DragInt((const char*)u8"        Счётчик тактов", (int*)&mm.ctx.counter, 0, 0, 0, "%d");
	mm.ctx.counter = mm.ctx.counter % 1'000'000;
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

void GUI::settingsWindow() {
	ImGui::SetNextWindowSize({ 400,500 });
	if (isSettingsOpen) {
		ImGui::Begin((const char*)u8"Настройки",&isSettingsOpen, ImGuiWindowFlags_NoResize);
		if (ImGui::Button((const char*)u8"Сбросить настройки")) {
			resetSettings();
		}
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
