#include "ProgramConsoleManager.h"

void ProgramConsoleManager::clear() {
	std::lock_guard<std::mutex> lock(stateMutex);
	output.clear();
}

void ProgramConsoleManager::write(const std::string& utf8Text) {
	std::lock_guard<std::mutex> lock(stateMutex);
	output += utf8Text;
}

std::string ProgramConsoleManager::getOutput() {
	std::lock_guard<std::mutex> lock(stateMutex);
	return output;
}

void ProgramConsoleManager::beginInputRequest() {
	std::lock_guard<std::mutex> lock(stateMutex);
	inputValue.clear();
	inputWaiting = true;
	inputReady = false;
	inputCancelled = false;
}

void ProgramConsoleManager::pushInput(const std::string& utf8Text) {
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		if (!inputWaiting)
			return;
		inputValue = utf8Text;
		inputWaiting = false;
		inputReady = true;
	}
	inputCv.notify_all();
}

bool ProgramConsoleManager::waitForInput(std::string& utf8Text) {
	std::unique_lock<std::mutex> lock(stateMutex);
	inputCv.wait(lock, [&]() { return inputReady || inputCancelled; });
	if (inputCancelled) {
		inputCancelled = false;
		return false;
	}
	utf8Text = inputValue;
	inputValue.clear();
	inputReady = false;
	return true;
}

void ProgramConsoleManager::cancelInput() {
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		if (!inputWaiting)
			return;
		inputWaiting = false;
		inputReady = false;
		inputCancelled = true;
	}
	inputCv.notify_all();
}

bool ProgramConsoleManager::isInputWaiting() {
	std::lock_guard<std::mutex> lock(stateMutex);
	return inputWaiting;
}
