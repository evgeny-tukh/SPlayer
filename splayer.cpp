#include <windows.h>
#include "resource.h"
#include <cstdint>
#include <thread>
#include <chrono>
#include "tools.h"

struct Ctx {
    HWND mainWnd, sentenceEditor, portSelector, baudSelector, startStop, composeCRC, sendOnce;
    HINSTANCE instance;
    std::thread *runner;
    std::string port;
    uint32_t baud;
    bool started;
};

char const *CLASS_NAME = "SimplePlayerWnd";

void sendOnce (Ctx *ctx) {
    HANDLE port = CreateFile (
        ctx->port.c_str (),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    if (port != INVALID_HANDLE_VALUE) {
        char buffer [100];
        unsigned long bytesWritten;
        memset (buffer, 0, sizeof (buffer));
        if (GetWindowText (ctx->sentenceEditor, buffer, 83)) {
            std::string output (buffer);
            output += "\r\n";
            WriteFile (port, output.c_str (), output.length (), & bytesWritten, nullptr);
        }

        CloseHandle (port);
    } else {
        char msg [1000];
        sprintf (msg, "Unable to open %s, error %d", ctx->port.c_str (), GetLastError ());
        MessageBox (ctx->mainWnd, msg, "Error", MB_ICONEXCLAMATION);
    }
}

void runnerProc (Ctx *ctx) {
    HANDLE port = INVALID_HANDLE_VALUE;
    while (!ctx->mainWnd || IsWindow (ctx->mainWnd)) {
        if (ctx->started) {
            if (port == INVALID_HANDLE_VALUE) {
                port = CreateFile (
                    ctx->port.c_str (),
                    GENERIC_READ | GENERIC_WRITE,
                    0,
                    nullptr,
                    OPEN_EXISTING,
                    0,
                    nullptr
                );
            }
            if (port != INVALID_HANDLE_VALUE) {
                char buffer [100];
                unsigned long bytesWritten;
                memset (buffer, 0, sizeof (buffer));
                if (GetWindowText (ctx->sentenceEditor, buffer, 83)) {
                    std::string output (buffer);
                    output += "\r\n";
                    WriteFile (port, output.c_str (), output.length (), & bytesWritten, nullptr);
                }
            }
        } else if (port != INVALID_HANDLE_VALUE) {
            CloseHandle (port);
            port = INVALID_HANDLE_VALUE;
        }
        std::this_thread::sleep_for (std::chrono::milliseconds (50));
    }
}

HWND createControl (HWND parent, Ctx *ctx, const char *className, int x, int y, int width, int height, uint32_t style, uint32_t id, const char *text = "") {
    return CreateWindow (className, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, parent, (HMENU) (uint64_t) id, ctx->instance, 0);
}

void start (Ctx *ctx) {
    EnableWindow (ctx->portSelector, FALSE);
    EnableWindow (ctx->baudSelector, FALSE);
    EnableWindow (ctx->sendOnce, FALSE);
}

void stop (Ctx *ctx) {
    EnableWindow (ctx->portSelector, TRUE);
    EnableWindow (ctx->baudSelector, TRUE);
    EnableWindow (ctx->sendOnce, TRUE);
}

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

void updateBaud (Ctx *ctx) {
    int sel = (int) SendMessage (ctx->baudSelector, CB_GETCURSEL, 0, 0);
    if (sel >= 0) {
        ctx->baud = SendMessage (ctx->baudSelector, CB_GETITEMDATA, sel, 0);
    }
}

void updatePort (Ctx *ctx) {
    char item [256];
    int sel = (int) SendMessage (ctx->portSelector, CB_GETCURSEL, 0, 0);
    if (sel >= 0) {
        SendMessage (ctx->portSelector, CB_GETLBTEXT, sel, (LPARAM) item);
        ctx->port = item;
    }
}

void composeCRC (Ctx *ctx) {
    char text [256];
    GetWindowText (ctx->sentenceEditor, text, sizeof (text));
    if (text [0] == '!' || text [0] == '$') {
        char *asterisk = strchr (text, '*');

        if (!asterisk) {
            asterisk = text + strlen (text);
            asterisk [0] = '*';
            asterisk [1] = 'h';
            asterisk [2] = 'h';
            asterisk [3] = '\0';
        }

        uint8_t crc = text [1];
        for (size_t i = 2; text [i] != '*'; ++ i) {
            crc ^= (uint8_t) text [i];
        }
        sprintf (asterisk + 1, "%02X", crc);
        SetWindowText (ctx->sentenceEditor, text);
    }
}

void doCommand (HWND wnd, uint16_t cmd) {
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);
    switch (cmd) {
        case IDC_SEND_ONCE:
            sendOnce (ctx); break;
        case IDC_COMPOSE_CRC:
            composeCRC (ctx); break;
        case IDC_BAUD:
            updateBaud (ctx); break;
        case IDC_PORT:
            updatePort (ctx); break;
        case ID_EXIT:
            if (confirmExit (wnd)) DestroyWindow (wnd);
            break;
        case IDC_STARTSTOP:
            ctx->started = !ctx->started;
            SetWindowText (ctx->startStop, ctx->started ? "&Stop" : "&Start");
            if (ctx->started) {
                start (ctx);
            } else {
                stop (ctx);
            }
            break;
    }
}

void initWindow (HWND wnd, WPARAM param1, LPARAM param2) {
    CREATESTRUCTA *data = (CREATESTRUCTA *) param2;
    Ctx *ctx = (Ctx *) data->lpCreateParams;
    std::vector<uint32_t> bauds { 4800, 9600, 14400, 19200, 38400, 115200 };
    std::vector<std::string> sentenceTemplates {
        "$RATLL,01,5915.233,N,00915.234,E,001,115959.30,T,*hh",
        "$RATTM,01,1.2,031.2,T,10.1,026.3,T,,,K,T01,T,,115959.30,A*hh",
        "$RAGGA,115959.30,5915.233,N,00915.234,E,2,10,,,M,,M,,*hh",
    };

    SetWindowLongPtr (wnd, GWLP_USERDATA, (LONG_PTR) data->lpCreateParams);

    std::vector<std::string> ports;

    getSerialPortsList (ports);
    createControl (wnd, ctx, "STATIC", 10, 10, 120, 22, SS_SIMPLE, IDC_STATIC, "Sentence template");
    createControl (wnd, ctx, "STATIC", 10, 40, 120, 22, SS_SIMPLE, IDC_STATIC, "Port");
    createControl (wnd, ctx, "STATIC", 10, 70, 120, 22, SS_SIMPLE, IDC_STATIC, "Baud");
    ctx->sentenceEditor = createControl (wnd, ctx, "COMBOBOX", 140, 10, 530, 200, WS_BORDER | WS_TABSTOP | CBS_DROPDOWN, IDC_SENTENCE);
    ctx->portSelector = createControl (wnd, ctx, "COMBOBOX", 140, 40, 100, 200, CBS_DROPDOWNLIST | WS_TABSTOP, IDC_PORT);
    ctx->baudSelector = createControl (wnd, ctx, "COMBOBOX", 140, 70, 100, 200, CBS_DROPDOWNLIST | WS_TABSTOP, IDC_BAUD);
    ctx->startStop = createControl (wnd, ctx, "BUTTON", 140, 100, 100, 22, BS_AUTOCHECKBOX | BS_PUSHLIKE | WS_TABSTOP, IDC_STARTSTOP, "&Start");
    ctx->sendOnce = createControl (wnd, ctx, "BUTTON", 250, 100, 100, 22, WS_TABSTOP, IDC_SEND_ONCE, "Send &once");
    ctx->composeCRC = createControl (wnd, ctx, "BUTTON", 530, 40, 140, 22, WS_TABSTOP, IDC_COMPOSE_CRC, "Compose CRC");

    for (auto& sentence: sentenceTemplates) {
        SendMessage (ctx->sentenceEditor, CB_ADDSTRING, 0, (LPARAM) sentence.c_str ());
    }
    for (auto& port: ports) {
        SendMessage (ctx->portSelector, CB_ADDSTRING, 0, (LPARAM) port.c_str ());
    }
    for (auto baud: bauds) {
        auto item = SendMessage (ctx->baudSelector, CB_ADDSTRING, 0, (LPARAM) std::to_string (baud).c_str ());
        SendMessage (ctx->baudSelector, CB_SETITEMDATA, item, baud);
    }
    SendMessage (ctx->portSelector, CB_SETCURSEL, 0, 0);
    SendMessage (ctx->baudSelector, CB_SETCURSEL, 0, 0);
    SendMessage (ctx->sentenceEditor, CB_SETCURSEL, 0, 0);

    if (!ports.empty ()) ctx->port = ports [0];
    ctx->baud = bauds [0];
}

LRESULT CALLBACK wndProc (HWND wnd, UINT msg, WPARAM param1, LPARAM param2) {
    switch (msg) {
        case WM_CTLCOLORSTATIC: {
            HDC ctlDC = (HDC) param1;
            SetBkMode (ctlDC, TRANSPARENT);
            break;
        }
        case WM_CREATE:
            initWindow (wnd, param1, param2); break;
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
    Ctx ctx;
    
    ctx.mainWnd = nullptr;
    ctx.instance = instance;
    ctx.started = false;
    ctx.runner = new std::thread (runnerProc, & ctx);

    registerClass (instance);

    ctx.mainWnd = CreateWindow (
        CLASS_NAME,
        "Simple Player",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        700,
        300,
        HWND_DESKTOP,
        mainMenu,
        instance,
        & ctx
    );

    ShowWindow (ctx.mainWnd, SW_SHOW);
    UpdateWindow (ctx.mainWnd);

    MSG msg;

    while (GetMessage (& msg, 0, 0, 0)) {
        TranslateMessage (& msg);
        DispatchMessage (& msg);
    }

    DestroyMenu (mainMenu);

    if (ctx.runner->joinable ())
        ctx.runner->join ();

    return 0;
}