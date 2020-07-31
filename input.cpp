#include <iostream>
#include <cassert>

#include <Windows.h>
#include <xinput.h>
#pragma comment(lib, "Xinput.lib")

#define DIRECTINPUT_VERSION 0x0800
#include <Dinput.h>
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dinput8.lib")

#include "input.h"
#include "commands.h"

#define BUFSIZE 4096

static IDirectInput8* DIObj = NULL;
static LPDIRECTINPUTDEVICE8 controller = NULL;
static int nbuttons = 0;
static char* butmap = NULL;

static BOOL __stdcall printobject( LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef) {
	std::wcout << L"  " << lpddoi->tszName << std::endl;
	return DIENUM_CONTINUE;
}
static BOOL __stdcall printdevice(LPCDIDEVICEINSTANCEW lpddoi, LPVOID pvRef) {
	std::wcout << L"[Type " << std::hex << lpddoi->dwDevType << std::dec << L"] Instance name: " << lpddoi->tszInstanceName << L". Product name: " << lpddoi->tszProductName << std::endl;
	LPDIRECTINPUTDEVICE8  dev;
	if (FAILED(DIObj->CreateDevice(lpddoi->guidInstance, &dev, NULL))) {
		std::wcerr << L"Could not create DirectInput device.\n";
		return DIENUM_CONTINUE;
	}
	dev->EnumObjects(printobject, NULL, DIDFT_ALL);
	dev->Release();
	return DIENUM_CONTINUE;
}
static BOOL __stdcall registergamepad(LPCDIDEVICEINSTANCEW dev, LPVOID) {
	if ((dev->dwDevType & 0xFF) == DI8DEVTYPE_GAMEPAD || (dev->dwDevType & 0xFF) == DI8DEVTYPE_1STPERSON) { // PS controller sometimes registers as DI8DEVTYPE_1STPERSON
		std::wcerr << L"Found gamepad device (subtype: " << (dev->dwDevType >> 8 & 0xFF) << L"): " << dev->tszInstanceName << std::endl;
		if (FAILED(DIObj->CreateDevice(dev->guidInstance, &controller, NULL))) {
			controller = NULL;
			std::wcerr << L"Failed to create DirectInput device.\n";
			return DIENUM_CONTINUE;
		}
		DIDEVCAPS caps = { .dwSize = sizeof(DIDEVCAPS), };
		if (FAILED(controller->GetCapabilities(&caps))
			|| (nbuttons = caps.dwButtons) <= 8
			|| !(butmap = new char[nbuttons])) {
			controller = NULL;
			std::wcerr << L"Failed to map buttons on DirectInput device.\n";
			controller->Release();
			controller = NULL;
			nbuttons = 0;
		}
		for (int i = 0; i < nbuttons; i++) {
			butmap[i] = 0;
		}
		return DIENUM_STOP;
	}
	return DIENUM_CONTINUE;
}

bool DirectInputInit() {
	// Find a suitable device
	HMODULE me = GetModuleHandleW(NULL);
	if (FAILED(DirectInput8Create(me, DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&DIObj, NULL))) {
		std::wcerr << L"Failed to initialize DirectInput object.\n";
		return false;
	}
	DIObj->EnumDevices(DI8DEVCLASS_GAMECTRL, registergamepad, NULL, DIEDFL_ALLDEVICES);
	if (!controller) {
		std::wcerr << L"No suitable DirectInput device found.\n";
		DIObj->Release();
		return false;
	}

	// Set up device
	if (FAILED(controller->SetDataFormat(&c_dfDIJoystick))) {
		std::wcerr << L"Failed to set DirectInput device format.\n";
		delete[] butmap;
		controller->Release();
		DIObj->Release();
		return false;
	}
	struct DIPROPDWORD setbufsize = {
		.diph = { .dwSize = sizeof(struct DIPROPDWORD), .dwHeaderSize = sizeof(struct DIPROPHEADER), .dwObj = 0, .dwHow = DIPH_DEVICE },
		.dwData = BUFSIZE,
	};
	if (FAILED(controller->SetProperty(DIPROP_BUFFERSIZE, &setbufsize.diph))) {
		std::wcerr << L"Failed to set DirectInput device buffer size.\n";
		delete[] butmap;
		controller->Release();
		DIObj->Release();
		return false;
	}
	struct DIPROPRANGE setrange = {
		.diph = {.dwSize = sizeof(struct DIPROPRANGE), .dwHeaderSize = sizeof(struct DIPROPHEADER), .dwObj = 0, .dwHow = DIPH_DEVICE },
		.lMin = -100,
		.lMax = 100,
	};
	struct DIPROPDWORD setdeadzone = {
		.diph = {.dwSize = sizeof(struct DIPROPDWORD), .dwHeaderSize = sizeof(struct DIPROPHEADER), .dwObj = 0, .dwHow = DIPH_DEVICE },
		.dwData = 600, // 6%
	};
	if (FAILED(controller->SetProperty(DIPROP_RANGE, &setrange.diph)) || FAILED(controller->SetProperty(DIPROP_DEADZONE, &setdeadzone.diph))) {
		std::wcerr << L"Failed to set range or deadzone for DirectInput device.\n";
		delete[] butmap;
		controller->Release();
		DIObj->Release();
		return false;
	}
	if (FAILED(controller->Acquire())) {
		std::wcerr << L"Failed to acquire DirectInput device.\n";
		delete[] butmap;
		controller->Release();
		DIObj->Release();
		return false;
	}
	return true;
}

bool DirectInputUninit() {
	if (butmap) {
		delete[] butmap;
	}
	if (controller) {
		controller->Release();
	}
	if (DIObj) {
		DIObj->Release();
	}
	return true;
}

void DirectInputPoll(int ms) {
	static DIDEVICEOBJECTDATA buf[BUFSIZE] = {};
	while (true) {
		// Get button presses via buffered data
		DWORD size = BUFSIZE;
		controller->Poll();
		HRESULT hres = controller->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), buf, &size, 0);
		if (hres == DIERR_INPUTLOST) {
			std::wcerr << L"PANIC: DirectInput lost." << std::endl;
			panic();
			return;
		}
		else if (hres == DI_BUFFEROVERFLOW) {
			std::wcerr << L"WARNING: DirectInput queue overflowed. Consider increasing buffer size." << std::endl;
		}
		for (unsigned int i = 0; i < size; i++) {
			if (buf[i].dwOfs == DIJOFS_X) {
				// std::wcout << L"X-Axis data: " << (int)buf[i].dwData << std::endl;
				continue;
			}
			if (buf[i].dwOfs == DIJOFS_Y) {
				// std::wcout << L"Y-Axis data: " << (int)buf[i].dwData << std::endl;
				continue;
			}
			if (buf[i].dwOfs == DIJOFS_Z) {
				// std::wcout << L"Z-Axis data: " << (int)buf[i].dwData << std::endl;
				continue;
			}
			if (buf[i].dwOfs == DIJOFS_RX) {
				// std::wcout << L"RX-Axis data: " << (int)buf[i].dwData << std::endl;
				continue;
			}
			if (buf[i].dwOfs == DIJOFS_RY) {
				// std::wcout << L"RY-Axis data: " << (int)buf[i].dwData << std::endl;
				continue;
			}
			if (buf[i].dwOfs == DIJOFS_RZ) {
				// std::wcout << L"RZ-Axis data: " << (int)buf[i].dwData << std::endl;
				continue;
			}
			if (buf[i].dwOfs == DIJOFS_POV(0)) {
				// std::wcout << L"D-PAD: " << (int)buf[i].dwData << std::endl;
				continue;
			}
			for (int j = 0; j < nbuttons; j++) {
				if (buf[i].dwOfs == DIJOFS_BUTTON(j)) {
					if (!(buf[i].dwData & 0x80)) {
						// std::wcout << L"Button " << j << " up." << std::endl;
						butmap[j] = 0;
					}
					else {
						// std::wcout << L"Button " << j << " down." << std::endl;
						butmap[j] = 1;
						// do button press processing
						if (j == 1) {
							land();
						}
						if (j == 2) {
							querybattery();
						}
						else if (j == 3) {
							takeoff();
						}
						else if (4 <= j && j <= 7) {
							if (butmap[4] && butmap[5] && butmap[6] && butmap[7]) {
								emergency();
							}
						}
					}
					break;
				}
			}
		}

		// Get stick position via immediate data
		DIJOYSTATE state;
		controller->Poll();
		hres = controller->GetDeviceState(sizeof(DIJOYSTATE), &state);
		if (hres == DIERR_INPUTLOST) {
			std::wcerr << L"PANIC: DirectInput lost." << std::endl;
			panic();
			return;
		}
		rc((int)state.lZ, -(int)state.lRz, -(int)state.lY, (int)state.lX);
		Sleep(ms);
	}
}

static DWORD UserIndex;
static XINPUT_STATE xinputstate[2]; // ringbuffer of size 2

bool XInputInit() {
	for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
		DWORD err = XInputGetState(i, &xinputstate[0]);
		if (err == ERROR_SUCCESS) {
			std::wcerr << L"Found XInput device. User index: " << i << std::endl;
			UserIndex = i;
			return true;
		}
		if (err == ERROR_DEVICE_NOT_CONNECTED) {
			continue;
		}
		std::wcerr << L"Unable to get XInput state: " << err << std::endl;
		return false;
	}
	std::wcerr << L"No XInput device found." << std::endl;
	return false;
}
bool XInputUninit() {
	return true; // lol
}
void XInputPoll(int ms) {
	int cur = 1;
	int prev = 0;
	int roll = 0, pitch = 0, throttle = 0, yaw = 0;
	while (true) {
		DWORD err = XInputGetState(UserIndex, &xinputstate[cur]);
		if (err != ERROR_SUCCESS) {
			if (err == ERROR_DEVICE_NOT_CONNECTED) {
				// try to look for a new device
				if (!XInputInit()) {
					std::wcerr << L"PANIC: XInput lost." << std::endl;
					panic();
					return;
				}
				cur = 1;
				prev = 0;
			}
			else {
				std::wcerr << L"Unable to get XInput state: " << err << std::endl;
			}
			continue;
		}
		if (xinputstate[cur].dwPacketNumber != xinputstate[prev].dwPacketNumber) {
			// state has changed
			if ((xinputstate[cur].Gamepad.wButtons & XINPUT_GAMEPAD_A) && !(xinputstate[prev].Gamepad.wButtons & XINPUT_GAMEPAD_A)) {
				land();
			}
			if ((xinputstate[cur].Gamepad.wButtons & XINPUT_GAMEPAD_B) && !(xinputstate[prev].Gamepad.wButtons & XINPUT_GAMEPAD_B)) {
				querybattery();
			}
			if ((xinputstate[cur].Gamepad.wButtons & XINPUT_GAMEPAD_Y) && !(xinputstate[prev].Gamepad.wButtons & XINPUT_GAMEPAD_Y)) {
				takeoff();
			}
			if ((xinputstate[cur].Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
				&& (xinputstate[cur].Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
				&& (xinputstate[cur].Gamepad.bLeftTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
				&& (xinputstate[cur].Gamepad.bRightTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
				&& !((xinputstate[prev].Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
					&& (xinputstate[prev].Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
					&& (xinputstate[prev].Gamepad.bLeftTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
					&& (xinputstate[prev].Gamepad.bRightTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD))) {
				emergency();
			}
			if (xinputstate[cur].Gamepad.sThumbLX <= -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
				yaw = 100 * (xinputstate[cur].Gamepad.sThumbLX + XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / (32768 - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			}
			else if (xinputstate[cur].Gamepad.sThumbLX >= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
				yaw = 100 * (xinputstate[cur].Gamepad.sThumbLX - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / (32767 - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			}
			else {
				yaw = 0;
			}
			if (xinputstate[cur].Gamepad.sThumbLY <= -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
				throttle = 100 * (xinputstate[cur].Gamepad.sThumbLY + XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / (32768 - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			}
			else if (xinputstate[cur].Gamepad.sThumbLY >= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
				throttle = 100 * (xinputstate[cur].Gamepad.sThumbLY - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / (32767 - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			}
			else {
				throttle = 0;
			}
			if (xinputstate[cur].Gamepad.sThumbRX <= -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
				roll = 100 * (xinputstate[cur].Gamepad.sThumbRX + XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) / (32768 - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
			}
			else if (xinputstate[cur].Gamepad.sThumbRX >= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
				roll = 100 * (xinputstate[cur].Gamepad.sThumbRX - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) / (32767 - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
			}
			else {
				roll = 0;
			}
			if (xinputstate[cur].Gamepad.sThumbRY <= -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
				pitch = 100 * (xinputstate[cur].Gamepad.sThumbRY + XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) / (32768 - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
			}
			else if (xinputstate[cur].Gamepad.sThumbRY >= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
				pitch = 100 * (xinputstate[cur].Gamepad.sThumbRY - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) / (32767 - XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
			}
			else {
				pitch = 0;
			}
		}
		rc(roll, pitch, throttle, yaw);
		prev = cur;
		cur = (cur + 1) % 2;
		Sleep(ms);
	}
}