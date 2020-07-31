#include <iostream>

#include <Windows.h>

#include "input.h"
#include "commands.h"

int main() {
	std::wcerr << L"Looking for XInput device:" << std::endl;
	if (XInputInit()) {
		if (!CommandsInit()) {
			MessageBoxW(NULL, L"Could not connect to tello. Make sure the correct Wi-Fi is selected.", L"No response from tello.", MB_OK);
			return 1;
		}
		XInputPoll(10);
		CommandsUninit();
		XInputUninit();
		return 0;
	}
	std::wcerr << L"Trying DirectInput instead:" << std::endl;
	if (DirectInputInit()) {
		if (!CommandsInit()) {
			MessageBoxW(NULL, L"Could not connect to tello. Make sure the correct Wi-Fi is selected.", L"No response from tello.", MB_OK);
			return 1;
		}
		DirectInputPoll(10);
		CommandsUninit();
		DirectInputUninit();
		return 0;
	}
	MessageBoxW(NULL, L"Could not find any input device. Make sure your device is connected and try again.", L"No input device.", MB_OK);
	return 1;
}