#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>

class ProgramConsoleManager {
public:
	inline static ProgramConsoleManager& getProgramConsoleManager() {
		static ProgramConsoleManager consoleManager;
		return consoleManager;
	}

	void clear();
	void write(const std::string& utf8Text);
	std::string getOutput();
	void beginInputRequest();
	void pushInput(const std::string& utf8Text);
	bool waitForInput(std::string& utf8Text);
	void cancelInput();
	bool isInputWaiting();

private:
	ProgramConsoleManager() = default;

	std::mutex stateMutex;
	std::condition_variable inputCv;
	std::string output;
	std::string inputValue;
	bool inputWaiting = false;
	bool inputReady = false;
	bool inputCancelled = false;
};
