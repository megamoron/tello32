#include <cassert>
#include <cstdio>
#include <ctime>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

static SOCKET sock = INVALID_SOCKET;
static WSAEVENT ev[] = {
	WSA_INVALID_EVENT, // data is ready to be received in sock 
	WSA_INVALID_EVENT, // sock is going to be closed (manually signaled: closesocket must not be called concurrently with another Winsock function call)
};

static CRITICAL_SECTION CSSend, CSPrint;

static HANDLE PrintThread;
static DWORD WINAPI PrintThreadProc(_In_ LPVOID); // thread procedure

bool CommandsInit() {
	// Initialize Winsock
	WSADATA wsaData;
	int startup = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (startup) {
		fprintf(stderr, "Failed to initialize Winsock: %d\n", startup);
		return false;
	}

	// set up socket and connect
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET) {
		fprintf(stderr, "Failed to create socket: %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}
	if ((ev[0] = WSACreateEvent()) == WSA_INVALID_EVENT
		|| (ev[1] = WSACreateEvent()) == WSA_INVALID_EVENT
		|| WSAEventSelect(sock, ev[0], FD_READ) == SOCKET_ERROR) {
		fprintf(stderr, "Failed to initialize socket events: %d\n", WSAGetLastError());
		if (ev[1] != WSA_INVALID_EVENT) {
			WSACloseEvent(ev[1]);
		}
		if (ev[0] != WSA_INVALID_EVENT) {
			WSACloseEvent(ev[0]);
		}
		closesocket(sock);
		WSACleanup();
		return false;
	}
	sockaddr_in me {
		.sin_family = AF_INET,
		.sin_port = htons(9000), // fix local port number
	};
	me.sin_addr.S_un.S_addr = INADDR_ANY;
	sockaddr_in tello {
		.sin_family = AF_INET,
		.sin_port = htons(8889),
	};
	InetPtonW(tello.sin_family, L"192.168.10.1", (PVOID)&tello.sin_addr);
	if (bind(sock, (const struct sockaddr*)&me, sizeof(struct sockaddr)) == SOCKET_ERROR
		|| connect(sock, (const struct sockaddr*)&tello, sizeof(struct sockaddr))) {
		fprintf(stderr, "Failed to connect to socket: %d\n", WSAGetLastError());
		WSACloseEvent(ev[1]);
		WSACloseEvent(ev[0]);
		closesocket(sock);
		WSACleanup();
		return false;
	}

	// send ''command'' and wait for confirmation
	DWORD timeout;
	for (timeout = 8ul; timeout <= 5000ul; timeout *= 5ul) {
		char cmd[] = "command";
		send(sock, cmd, sizeof(cmd) - 1, 0); // don't send trailing '\0'
		char msg[2] = {};
		if (WSAWaitForMultipleEvents(1, ev, FALSE, timeout, FALSE) != WSA_WAIT_TIMEOUT) {
			if (recv(sock, msg, 2, 0) == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET) {
				fprintf(stderr, "Connection refused.\n");
				shutdown(sock, SD_BOTH);
				WSACloseEvent(ev[1]);
				WSACloseEvent(ev[0]);
				closesocket(sock);
				WSACleanup();
				return false;
			}
			else {
				WSAResetEvent(ev[0]);
				if (msg[0] == 'o' && msg[1] == 'k') {
					break;
				}
			}
		}
	}
	if (timeout == 25000ul) {
		fprintf(stderr, "Could not get response from tello.\n");
		shutdown(sock, SD_BOTH);
		WSACloseEvent(ev[1]);
		WSACloseEvent(ev[0]);
		closesocket(sock);
		WSACleanup();
		return false;
	}

	// setup synchronization datastructures and spawn thread
	InitializeCriticalSection(&CSPrint);
	InitializeCriticalSection(&CSSend);
	PrintThread = CreateThread(NULL, 0, PrintThreadProc, NULL, 0, NULL);
	if (!PrintThread) {
		fprintf(stderr, "Could not create thread: %d\n", (int)GetLastError());
		DeleteCriticalSection(&CSSend);
		DeleteCriticalSection(&CSPrint);
		shutdown(sock, SD_BOTH);
		WSACloseEvent(ev[1]);
		WSACloseEvent(ev[0]);
		closesocket(sock);
		WSACleanup();
		return false;
	}

	return true;
}

bool CommandsUninit() {
	// terminate thread
	DWORD texit = 1ul;
	WSASetEvent(ev[1]);
	WaitForSingleObject(PrintThread, INFINITE);
	GetExitCodeThread(PrintThread, &texit);
	CloseHandle(PrintThread);

	// clean up synchronization datastructures
	DeleteCriticalSection(&CSSend);
	DeleteCriticalSection(&CSPrint);

	// close socket
	shutdown(sock, SD_BOTH);
	WSACloseEvent(ev[1]);
	WSACloseEvent(ev[0]);
	closesocket(sock);
	WSACleanup();

	return texit == 0;
}


static DWORD WINAPI PrintThreadProc(_In_ LPVOID) {
	while (WSAWaitForMultipleEvents(2, ev, FALSE, WSA_INFINITE, FALSE) != WSA_WAIT_EVENT_0+1) {
		// Receive
		char buf[1519]; // according to the Tello3.py sample program
		int bytes = recv(sock, buf, 1518, 0);
		if (bytes == SOCKET_ERROR) {
			int error = WSAGetLastError();
			EnterCriticalSection(&CSPrint);
			fprintf(stderr, " --> Failed to recieve: %d\n", error);
			LeaveCriticalSection(&CSPrint);
			return error;
		}
		WSAResetEvent(ev[0]);

		// print
		buf[bytes] = '\0';
		if (bytes >= 1 && buf[bytes - 1] == '\n') {
			buf[bytes - 1] = '\0';
		}
		time_t t = time(NULL);
		struct tm tm;
		localtime_s(&tm, &t);
		EnterCriticalSection(&CSPrint);
		printf("[%02d:%02d:%02d]: --> %s\n", tm.tm_hour, tm.tm_min, tm.tm_sec, buf);
		LeaveCriticalSection(&CSPrint);
	}

	return 0;
}

static void SendAsync(const char *buf, size_t len) {
	assert(buf[len] == '\0');
	if (len >= 2 && buf[0] == 'r' && buf[1] == 'c') {
		// don't print before sending
		EnterCriticalSection(&CSSend);
		{
			if (send(sock, buf, len, 0) == SOCKET_ERROR) {
				fprintf(stderr, "Failed to send: %d\n", WSAGetLastError());
			}
		}
		LeaveCriticalSection(&CSSend);
	}
	else {
		// print before sending
		time_t t = time(NULL);
		struct tm tm;
		localtime_s(&tm, &t);
		EnterCriticalSection(&CSPrint);
		{
			printf("[%02d:%02d:%02d]: %s\n", tm.tm_hour, tm.tm_min, tm.tm_sec, buf);
			EnterCriticalSection(&CSSend);
			{
				if (send(sock, buf, len, 0) == SOCKET_ERROR) {
					fprintf(stderr, "Failed to send: %d\n", WSAGetLastError());
				}
			}
			LeaveCriticalSection(&CSSend);
		}
		LeaveCriticalSection(&CSPrint);
	}
}

void takeoff() {
	char cmd[] = "takeoff";
	SendAsync(cmd, sizeof(cmd) - 1); // don't send '\0'
}
void land() {
	char cmd[] = "land";
	SendAsync(cmd, sizeof(cmd) - 1); // don't send '\0'
}
void emergency() {
	char cmd[] = "emergency";
	SendAsync(cmd, sizeof(cmd) - 1); // don't send '\0'
}
void speed(int x) {
	assert(10 <= x && x <= 100);
	char cmd[10];
	int len = snprintf(cmd, 10, "speed %d", x); // len does not count '\0'
	SendAsync(cmd, len);
}
void querybattery() {
	char cmd[] = "battery?";
	SendAsync(cmd, sizeof(cmd) - 1); // don't send '\0'
}
void rc(int a, int b, int c, int d) {
	assert(-100 <= a && a <= 100);
	assert(-100 <= b && b <= 100);
	assert(-100 <= c && c <= 100);
	assert(-100 <= d && d <= 100);
	char cmd[23];
	int len = snprintf(cmd, 24, "rc %d %d %d %d", a, b, c, d); // len does not count '\0'
	SendAsync(cmd, len);
}
void panic() {
	// panic
	land();
	Sleep(100);
	land();
	Sleep(500);
	land();
	Sleep(2500);
	land();
	Sleep(12500);
	land();
	emergency();
}