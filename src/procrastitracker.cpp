
#include "stdafx.h"
#include "resource.h"
#include "IdleTracker.h"

#include "win32tools.h"
#include "inputdlg.h"

#include "slaballoc.h"
SlabAlloc pool;
#include "tools.h"
StringPool strpool;

#ifdef __MINGW32__
#define sprintf_s snprintf
#endif

DWORD mainthreadid = 0;

void warn(char *what) { MessageBoxA(NULL, what, "procrastitracker", MB_OK); }

void panic(char *what) {
    warn(what);
    exit(0);
}

typedef BOOL(WINAPI *QFPIN)(HANDLE, DWORD, wchar_t *, PDWORD);
QFPIN qfpin = NULL;

DWORD lastsavetime = 0;
bool changesmade = false;
DWORD awaysecsdialog = 0;

DWORD maxsecondsforbargraph = 100000;
DWORD bargraphwidth = 100;

int starttime, endtime, firstday;

DWORD foldlevel = 1;
int filterontag = -1;

struct tag {
    char name[32];
    DWORD color;
    HBRUSH br;
    HBITMAP barbm;
};

const int MAXTAGS = 15;

tag tags[MAXTAGS] = {
    {"UNTAGGED", 0xD0D0D0, NULL, NULL},      {"work", 0x60FF60, NULL, NULL},
    {"games", 0x6060FF, NULL, NULL},         {"surfing", 0x60FFFF, NULL, NULL},
    {"entertainment", 0xFF60FF, NULL, NULL}, {"communication", 0xFFFF60, NULL, NULL},
    {"organization", 0xFF6060, NULL, NULL},  {"project 1", 0x0000FF, NULL, NULL},
    {"project 2", 0x00FF00, NULL, NULL},     {"project 3", 0xFF0000, NULL, NULL},
    {"project 4", 0xB0B060, NULL, NULL},     {"project 5", 0xB060B0, NULL, NULL},
    {"project 6", 0x60B0B0, NULL, NULL},     {"project 7", 0x606060, NULL, NULL},
    {"project 8", 0xB0B0B0, NULL, NULL},
};

const int FILE_FORMAT_VERSION = 13;

extern char databasemain[];
extern char databaseback[];

void PANIC() {
    // Try to restore the latest backup
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (GetFileAttributesExA(databaseback, GetFileExInfoStandard, &attr) &&
        !(attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        SYSTEMTIME stUTC, stLocal;
        FileTimeToSystemTime(&attr.ftLastWriteTime, &stUTC);
        SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
        char msg[256];
        sprintf_s(msg, 256,
                  "The database is corrupt. Last available backup: %04d-%02d-%02d %02d:%02d\n"
                  "Do you want to restore it?",
                  stLocal.wYear, stLocal.wMonth, stLocal.wDay, stLocal.wHour, stLocal.wMinute);

        if (MessageBoxA(NULL, msg, "DB problem", MB_YESNO) == IDYES) {
            DeleteFile(databasemain);
            if (CopyFileA(databaseback, databasemain, TRUE)) {
                MessageBoxA(NULL, "Backup restored. Please restart the program.", "Backup restored",
                            MB_OK);
                ExitProcess(0);
            }
        }
    }
    MessageBoxA(
        NULL,
        "The database is corrupt. Please restore the last backup (ProcrastiTracker makes these "
        "frequently), "
        "and try again. See Procrastitracker start menu for database location. Exiting program..",
        "DB problem", 0);
    exit(1);
}

void gzread_s(gzFile f, void *buf, uint size) {
    if (gzread(f, buf, size) < (int)size) {
        gzclose(f);
        PANIC();
    }
}
int gzgetc_s(gzFile f) {
    int c = gzgetc(f);
    if (c == -1) PANIC();
    return c;
}

void gzwrite_s(gzFile file, voidpc buf, unsigned len) {
    if (len != gzwrite(file, buf, len)) panic("PT: write failed while writing database to disk");
}

void wint(gzFile f, int i) { gzwrite_s(f, &i, sizeof(int)); }

int rint(gzFile f) {
    int i;
    gzread_s(f, &i, sizeof(int));
    return i;
}

char *rstr(gzFile f) {
    String s;
    for (int c; c = gzgetc_s(f); s.Cat((uchar)c))
        ;
    return strpool.add(s);
}

enum {
    PREF_SAMPLE = 0,
    PREF_IDLE,
    PREF_SEMIIDLE,
    PREF_AUTOSAVE,
    PREF_CULL,
    PREF_AWAY,
    PREF_XINPUTACTIVITYFREQUENCY,
    PREF_XINPUTACTIVITY,
    PREF_FOREGROUNDFULLSCREEN,
    PREF_AWAYAUTO,
    NUM_PREFS
};  // new entries need to bump fileformat

numeditbox minfilter = {NULL, 0, 0, 60 * 24, IDC_EDIT2};

DWORD timer_sample_val = 0;  // May be different from PREF_SAMPLE since timer doesn't get reset.
numeditbox prefs[NUM_PREFS] = {
    {NULL, 5, 1, 60, IDC_EDIT1},
    {NULL, 180, 5, 3600, IDC_EDIT3},
    {NULL, 10, 5, 60, IDC_EDIT4},
    {NULL, 10, 1, 60, IDC_EDIT5},
    {NULL, 300, 0, 3600, IDC_EDIT6},
    {NULL, 0, 0, 9999, IDC_EDIT7},
    {NULL, 5, 1, 60, IDC_EDIT_XINPUTACTIVITY_FREQUENCY},
    {NULL, 0, 0, 1, IDC_CHECK_XINPUTACTIVITY, true},
    {NULL, 1, 0, 1, IDC_CHECK_FOREGROUNDFULLSCREEN, true},
    {NULL, 0, 0, 9999, IDC_EDIT10},
};

int statnodes = 0, statdays = 0, statht = 0, statleaf = 0, statone = 0;
char filterstrcontents[100] = "";

#include "day.h"
#include "node.h"

node *root = new node("(root)", NULL);
char databaseroot[MAX_PATH];
char databasemain[MAX_PATH];
char databaseback[MAX_PATH];
char databasetemp[MAX_PATH];

#include "nodedb.h"
#include "ddeutil.h"

HINSTANCE hInst;
HWND mainhwnd = NULL;
HWND awaydialog = NULL;
HWND treeview = NULL;
HWND tagpicker = NULL;
HWND taglist = NULL;
HWND taglistedit = NULL;
HWND foldslider = NULL;
HWND secondsfilteredit = NULL;
HWND startingdatepicker = NULL;
HWND endingdatepicker = NULL;
HWND keyedit = NULL;
HWND quickcombo = NULL;
HWND filterstr = NULL;
HDC bitmapdc = NULL;
HIMAGELIST tagimages = NULL;
HBRUSH whitebrush = NULL, greybrush = NULL;
node *selectednode = NULL;
node *prevselectednode = NULL;

const int bmsize = 16;
const int daygraphwidth = 200;

#include "away.h"
#include "xinputactivity.h"
#include "timercallback.h"
#include "statview.h"
#include "prefview.h"
#include "regview.h"

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG:
            SendMessageA(hDlg, WM_SETICON, 0,
                         (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_PROCRASTITRACKER)));
            SetWindowTextA(GetDlgItem(hDlg, IDC_STATIC1), "ProcrastiTracker Version " __DATE__);
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

UINT WM_TASKBARCREATED = 0;

bool CreateTaskBarIcon(HWND hWnd, DWORD action, int repeats) {
    NOTIFYICONDATA nid;
    ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_PROCRASTITRACKER));
    strcpy(nid.szTip, "procrastitracker (active)");  // max 64 chars
    loop(i, repeats) {
        if (Shell_NotifyIcon(action, &nid)) return true;
        Sleep(1000);
    }
    return false;
}

BOOL FileRequest(HWND hWnd, char *requestfilename, size_t reqlen, char *defaultname, char *exts,
                 char *title, bool doexport = true, bool multi = false) {
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    strcpy(requestfilename, defaultname);
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.lpstrFile = requestfilename;
    ofn.nMaxFile = reqlen;
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_ENABLESIZING | OFN_PATHMUSTEXIST |
                (doexport ? 0 : OFN_FILEMUSTEXIST) | (multi ? 0 : OFN_ALLOWMULTISELECT);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = exts;
    ofn.lpstrTitle = title;
    return doexport ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;
    switch (message) {
        case WM_COMMAND:
            wmId = LOWORD(wParam);
            wmEvent = HIWORD(wParam);
            // inputhookcleanup();
            switch (wmId) {
                case 'ST':
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_PROPPAGE_LARGE), hWnd, Stats);
                    break;
                case 'AS':
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_PROPPAGE_MEDIUM), hWnd, Prefs);
                    break;
                case 'AB': DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About); break;
                case 'EX': DestroyWindow(hWnd); break;
                case 'EH': {
                    recompaccum();
                    char requestfilename[1000];
                    if (FileRequest(hWnd, requestfilename, sizeof(requestfilename),
                                    "procrastitracker_report.html",
                                    "HTML Files\0*.html;*.htm\0All Files\0*.*\0\0",
                                    "Save Exported HTML File As..."))
                        exporthtml(requestfilename);
                    break;
                }
                case 'EF': {
                    recompaccum();
                    char requestfilename[1000];
                    if (FileRequest(hWnd, requestfilename, sizeof(requestfilename),
                                    "exported_view.PT",
                                    "ProcrastiTracker Database Files\0*.PT\0All Files\0*.*\0\0",
                                    "Save Exported Database View As..."))
                        save(true, requestfilename);
                    break;
                }
                case 'MD': {
                    char requestfilename[100000];  // Must fit many filenames.
                    if (FileRequest(hWnd, requestfilename, sizeof(requestfilename),
                                    "db_fromothercomputer.PT",
                                    "ProcrastiTracker Database Files\0*.PT\0All Files\0*.*\0\0",
                                    "Merge Database File Into Current Database...")) {
                        String dir(requestfilename);
                        char *it = requestfilename + dir.Len() + 1;
                        if (!*it) {  // Single filename.
                            OutputDebugF("merging single file: \"%s\"\n", requestfilename);
                            mergedb(requestfilename);
                        } else { // Multiple filenames.
                            dir.Cat("\\");
                            while (*it) {
                                String fn(dir.c_str());
                                int len = strlen(it);
                                fn.Cat(it, len);
                                it += len + 1;
                                OutputDebugF("merging multiple files: \"%s\"\n", fn.c_str());
                                mergedb(fn.c_str());
                            }
                        }
                    }
                    break;
                }
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            break;
        case WM_DESTROY: PostQuitMessage(0); break;
        case WM_USER + 1:  // tray icon
            if (lParam == WM_LBUTTONDBLCLK || lParam == WM_RBUTTONUP) {
                HMENU myMenu = CreatePopupMenu();
                if (!myMenu) break;

                AppendMenuA(myMenu, MF_STRING, 'ST', "View Statistics..");
                AppendMenuA(myMenu, MF_STRING, 'AS', "Advanced Settings..");
                AppendMenuA(myMenu, MF_STRING, 'EH', "Export HTML (view)..");
                AppendMenuA(myMenu, MF_STRING, 'EF', "Export database (view) ..");
                AppendMenuA(myMenu, MF_STRING, 'MD', "Merge in database..");
                AppendMenuA(myMenu, MF_STRING, 'AB', "About..");
                AppendMenuA(myMenu, MF_STRING, 'EX', "Exit");

                POINT pt;
                GetCursorPos(&pt);

                SetForegroundWindow(hWnd);
                TrackPopupMenu(myMenu, 0, pt.x, pt.y, 0, hWnd, NULL);
                SendMessage(hWnd, WM_NULL, 0, 0);
                DestroyMenu(myMenu);
            }
            break;
        case WM_APP + 1:
            // Custom message received from hook thread that we're back from idle and
            // within the idle dialog time.
            TrackAwayTime(lParam);
            break;
        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hWnd, SW_HIDE);
                return 0;
            }
        // Fix from Alexandra Kalicinska: save the database before terminating
        // the program due to user logout or system reboot or system shutdown
        // (the GetMessage() loop in _tWinMain() does not get the signal;
        //  here the program gets first WM_QUERYENDSESSION, then WM_ENDSESSION;
        //  and return from the second terminates the program immediately)
        case WM_QUERYENDSESSION: save(); return TRUE;
        // case WM_ENDSESSION:
        default:
            if (message == WM_TASKBARCREATED) { // explorer has restarted
                // See:
                // http://twigstechtips.blogspot.com/2011/02/c-detect-when-windows-explorer-has.html
                Sleep(10000);  // Apparently if we re-create it too quickly, it will fail.
                // First try modify, since apparently we can get WM_TASKBARCREATED even
                // if the icon wasn't actually destroyed.
                if (!CreateTaskBarIcon(hWnd, NIM_MODIFY, 3) &&
                    !CreateTaskBarIcon(hWnd, NIM_ADD, 10)) {
                    // This can still fail repeatedly under certain conditions in fullscreen
                    // games, causing annoying popups, so let's no warn at all.
                    //warn("PT: Cannot recreate task bar icon");
                }
            }
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void MakeDPIAware() {
    #ifdef _WIN32
        // Without this, Windows scales the window if scaling is set in display settings.
        #ifndef DPI_ENUMS_DECLARED
            typedef enum PROCESS_DPI_AWARENESS
            {
                PROCESS_DPI_UNAWARE = 0,
                PROCESS_SYSTEM_DPI_AWARE = 1,
                PROCESS_PER_MONITOR_DPI_AWARE = 2
            } PROCESS_DPI_AWARENESS;
        #endif

        typedef BOOL (WINAPI * SETPROCESSDPIAWARE_T)(void);
        typedef HRESULT (WINAPI * SETPROCESSDPIAWARENESS_T)(PROCESS_DPI_AWARENESS);
        HMODULE shcore = LoadLibraryA("Shcore.dll");
        SETPROCESSDPIAWARENESS_T SetProcessDpiAwareness = NULL;
        if (shcore) {
            SetProcessDpiAwareness =
                (SETPROCESSDPIAWARENESS_T)GetProcAddress(shcore, "SetProcessDpiAwareness");
        }
        HMODULE user32 = LoadLibraryA("User32.dll");
        SETPROCESSDPIAWARE_T SetProcessDPIAware = NULL;
        if (user32) {
            SetProcessDPIAware =
                (SETPROCESSDPIAWARE_T)GetProcAddress(user32, "SetProcessDPIAware");
        }

        if (SetProcessDpiAwareness) {
            SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        } else if (SetProcessDPIAware) {
            SetProcessDPIAware();
        }

        if (user32) FreeLibrary(user32);
        if (shcore) FreeLibrary(shcore);
    #endif
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine,
                       int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    MakeDPIAware();

    mainthreadid = GetCurrentThreadId();
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (kernel32) qfpin = (QFPIN)GetProcAddress(kernel32, "QueryFullProcessImageNameW");
    INITCOMMONCONTROLSEX icc = {sizeof(INITCOMMONCONTROLSEX),
                                ICC_BAR_CLASSES | ICC_DATE_CLASSES | ICC_LISTVIEW_CLASSES |
                                    ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES |
                                    ICC_USEREX_CLASSES | ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);
    HWND hWnd = FindWindowA("PROCRASTITRACKER", NULL);
    if (hWnd) {
        // show the stats view of the existing instance
        WPARAM wParam = MAKEWPARAM('ST', 0);
        SendMessageA(hWnd, WM_COMMAND, wParam, NULL);
        return 0;
    }
    if (!ddeinit()) panic("PT: Cannot initialize DDE");
    // This is for chrome only:
    eventhookinit();
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PROCRASTITRACKER));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;  // MAKEINTRESOURCE(IDC_PROCRASTITRACKER);
    wcex.lpszClassName = "PROCRASTITRACKER";
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_PROCRASTITRACKER));
    RegisterClassEx(&wcex);
    hInst = hInstance;
    whitebrush = CreateSolidBrush(0xFFFFFF);
    greybrush = CreateSolidBrush(0xb99d7f);
    mainhwnd = CreateWindow("PROCRASTITRACKER", "procrastitracker", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, 0, 300, 150, NULL, NULL, hInstance, NULL);
    if (!mainhwnd) panic("PT: Cannot create main window");
    if ((WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"))) == 0)
        panic("PT: Cannot register TaskbarCreated message");
    if (!CreateTaskBarIcon(mainhwnd, NIM_ADD, 10)) panic("PT: Cannot create task bar icon");
    ShowWindow(mainhwnd, SW_HIDE /*nCmdShow*/);
    UpdateWindow(mainhwnd);
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, databaseroot))) {
        PathAppend(databaseroot, "\\");
    } else {
        // should not happen, but if it does, databases will just end up in current dir
        databaseroot[0] = 0;
    }
    PathAppend(databaseroot, "procrastitrackerdbs\\");
    SHCreateDirectoryEx(NULL, databaseroot, NULL);
    strcpy(databasemain, databaseroot);
    strcpy(databaseback, databaseroot);
    strcpy(databasetemp, databaseroot);
    PathAppend(databasemain, "db.PT");
    PathAppend(databaseback, "db_BACKUP.PT");
    PathAppend(databasetemp, "db_TEMP.~PT");
    starttime = now();
    endtime = now();
    load(root, databasemain, false);
    firstday = starttime;
    lastsavetime = GetTickCount();
    #ifdef MINGW32_BUG
    SetTimer(NULL, 0, prefs[PREF_SAMPLE].ival * 1000, timerfunc);
    #else
    SetTimer(NULL, NULL, prefs[PREF_SAMPLE].ival * 1000, timerfunc);
    #endif
    timer_sample_val = prefs[PREF_SAMPLE].ival;
    launchhookthread();
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    killhookthread();
    save();
    CreateTaskBarIcon(mainhwnd, NIM_DELETE, 3);
    ddeclean();
    eventhookclean();
    #ifdef _DEBUG
        delete root;
        strpool.clear();
    #endif
    return (int)msg.wParam;
}

#pragma comment( \
    linker,      \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
