/*
 * test/win_stub/windows.h
 *
 * TEST-ONLY stub of the Win32 console API, used on Linux to syntax-check
 * the _WIN32 branch of term.c with `make stub-win`. It is NOT a substitute
 * for the real headers: on Windows the genuine <windows.h> is used.
 *
 * Signatures mirror the real API as used by src/term.c.
 */
#ifndef WIN31_STUB_WINDOWS_H
#define WIN31_STUB_WINDOWS_H

typedef int BOOL;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned short WCHAR;
typedef short SHORT;
typedef void *HANDLE;
typedef DWORD *LPDWORD;
typedef unsigned long long DWORDLONG;
#define WINAPI
#define IN
#define OUT
#define TRUE 1
#define FALSE 0

#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_INPUT_HANDLE  ((DWORD)-10)

#define ENABLE_PROCESSED_INPUT 0x0001
#define ENABLE_ECHO_INPUT      0x0004
#define ENABLE_WINDOW_INPUT    0x0008
#define ENABLE_MOUSE_INPUT     0x0010
#define ENABLE_EXTENDED_FLAGS  0x0080
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004

#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT  258

#define VK_BACK    0x08
#define VK_TAB     0x09
#define VK_RETURN  0x0D
#define VK_ESCAPE  0x1B
#define VK_LEFT    0x25
#define VK_UP      0x26
#define VK_RIGHT   0x27
#define VK_DOWN    0x28
#define VK_DELETE  0x2E
#define VK_HOME    0x24
#define VK_END     0x23
#define VK_PRIOR   0x21
#define VK_NEXT    0x22
#define VK_F1      0x70
#define VK_F2      0x71
#define VK_F3      0x72
#define VK_F4      0x73
#define VK_F10     0x79

#define KEY_EVENT     1
#define MOUSE_EVENT   2

#define MOUSE_MOVED   0x0001
#define MOUSE_WHEELED 0x0400
#define LEFTDOWN      0x0010
#define LEFTUP        0x0020
#define RIGHTDOWN     0x2000
#define RIGHTUP       0x4000
#define MIDDLEDOWN    0x20000
#define MIDDLEUP      0x40000

typedef struct {
    SHORT X;
    SHORT Y;
} COORD, *LPCOORD;

typedef struct {
    WORD Left;
    WORD Top;
    WORD Right;
    WORD Bottom;
} SMALL_RECT;

typedef struct {
    COORD dwSize;
    COORD dwCursorPosition;
    WORD wAttributes;
    SMALL_RECT srWindow;
    COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO, *LPCONSOLE_SCREEN_BUFFER_INFO;

typedef struct {
    DWORD dwSize;
    BOOL bVisible;
} CONSOLE_CURSOR_INFO, *PCONSOLE_CURSOR_INFO;

typedef struct {
    BOOL bKeyDown;
    WORD wRepeatCount;
    WORD wVirtualKeyCode;
    WORD wVirtualScanCode;
    WCHAR UnicodeChar;
    WORD wScanCode;
} KEY_EVENT_RECORD;

typedef struct {
    DWORD dwButtonData;
    DWORD dwControlKeyState;
    DWORD dwEventFlags;
    DWORD dwMousePosition;
    DWORD dwButtonState;
} MOUSE_EVENT_RECORD;

typedef struct {
    DWORD dwEventType;
    union {
        KEY_EVENT_RECORD KeyEvent;
        MOUSE_EVENT_RECORD MouseEvent;
    } Event;
} INPUT_RECORD, *LPINPUT_RECORD;

HANDLE GetStdHandle(DWORD nStdHandle);
BOOL GetConsoleMode(HANDLE hConsoleHandle, LPDWORD lpMode);
BOOL SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode);
BOOL GetConsoleScreenBufferInfo(HANDLE hConsoleOutput,
                                LPCONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo);
BOOL GetConsoleCursorInfo(HANDLE hConsoleOutput, PCONSOLE_CURSOR_INFO lpConsoleCursorInfo);
BOOL SetConsoleCursorInfo(HANDLE hConsoleOutput, PCONSOLE_CURSOR_INFO lpConsoleCursorInfo);
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
BOOL ReadConsoleInput(HANDLE hInputHandle, LPINPUT_RECORD lpBuffer,
                      DWORD nLength, LPDWORD lpNumberOfEventsRead);
typedef BOOL(WINAPI *PSECURITY_NOTIFICATION_ROUTINE);
typedef BOOL(WINAPI *PHANDLER_ROUTINE)(DWORD);
BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE HandlerRoutine, BOOL Add);
void Sleep(DWORD dwMilliseconds);
DWORDLONG GetTickCount64(void);
int _stricmp(const char *s1, const char *s2);

#endif /* WIN31_STUB_WINDOWS_H */
