#include "Gui/GraphicsManager.h"
#include "Gui/GUI.h"
#include "Logic/MemoryManager.h"


int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
	MemoryManager::getMemoryManager().init();
	GraphicsManager& gm = GraphicsManager::getGraphicsManager();
	EmulatorManager& em = EmulatorManager::getEmulatorManager();
	em.init();
	gm.init();
	GUI::getGUI().loadSettings();
	while (!gm.proccessWndMsg())
		gm.render();
	gm.destroy();
	em.destroy();
	return 0;
}