#include <windows.h>
#include "resource.h"
#include <cstdint>

char const *CLASS_NAME = "SimplePlayerWnd";

bool confirmExit (HWND wnd) {
    return MessageBox (
        wnd,
        "Do you want to exit?",
        "Exit application",
        MB_YESNO | MB_ICONQUESTION
    ) == IDYES;
}

bool doSysCommand (HWND wnd, uint32_t cmd) {
    switch (cmd) {
        case SC_CLOSE:
            return confirmExit (wnd) ? false : true;
    }
    return false;
}

void doCommand (HWND wnd, uint16_t cmd) {
    switch (cmd) {
        case ID_EXIT:
            if (confirmExit (wnd)) DestroyWindow (wnd);
            break;
    }
}

LRESULT CALLBACK wndProc (HWND wnd, UINT msg, WPARAM param1, LPARAM param2) {
    switch (msg) {
        case WM_COMMAND:
            doCommand (wnd, LOWORD (param1)); break;
        case WM_DESTROY:
            PostQuitMessage (0); break;
        case WM_SYSCOMMAND:
            if (doSysCommand (wnd, param1)) break;
        default:
            return DefWindowProc (wnd, msg, param1, param2);
    }
    return 0;
}

bool registerClass (HINSTANCE instance) {
    WNDCLASSA classInfo;

    memset (& classInfo, 0, sizeof (classInfo));

    classInfo.hbrBackground = (HBRUSH) GetStockObject (WHITE_BRUSH);
    classInfo.hCursor = LoadCursor (nullptr, IDC_ARROW);
    classInfo.hIcon = LoadIcon (nullptr, IDI_APPLICATION);
    classInfo.hInstance = instance;
    classInfo.lpfnWndProc = wndProc;
    classInfo.lpszClassName = CLASS_NAME;
    classInfo.style = CS_VREDRAW | CS_HREDRAW;

    return RegisterClass (& classInfo) != 0;
}

int WINAPI WinMain (HINSTANCE instance, HINSTANCE prevInstance, char *cmdLine, int showCmd) {
    HMENU mainMenu = LoadMenu (instance, MAKEINTRESOURCE (IDR_MAINMENU));

    registerClass (instance);

    HWND mainWnd = CreateWindow (
        CLASS_NAME,
        "Simple Player",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        500,
        300,
        HWND_DESKTOP,
        mainMenu,
        instance,
        nullptr
    );

    ShowWindow (mainWnd, SW_SHOW);
    UpdateWindow (mainWnd);

    MSG msg;

    while (GetMessage (& msg, 0, 0, 0)) {
        TranslateMessage (& msg);
        DispatchMessage (& msg);
    }

    DestroyMenu (mainMenu);

    return 0;
}