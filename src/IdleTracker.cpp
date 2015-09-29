#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER         // Allow use of features specific to Windows XP or later.
#define WINVER 0x0501  // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT         // Allow use of features specific to Windows XP or later.
#define _WIN32_WINNT 0x0501  // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINDOWS         // Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410  // Change this to the appropriate value to target Windows Me or later.
#endif

#include <windows.h>
#define abs(x) ((x) < 0 ? -(x) : (x))  // don't want to have to rely on libc

HHOOK g_hHkKeyboardLL = NULL;  // handle to the keyboard hook
HHOOK g_hHkMouseLL = NULL;     // handle to the mouse hook
DWORD g_dwLastTick = 0;        // tick time of last input event
LONG g_mouseLocX = -1;         // x-location of mouse position
LONG g_mouseLocY = -1;         // y-location of mouse position
int key = 0, lmb = 0, rmb = 0, scr = 0;

extern void panic(char *);

// these next 2 functions really should go thru a mutex / critical section, but they're called from the main thread once
// every 5 seconds or so,
// and the worst that could happen is that the main thread context switches in the middle of inputhookstats and the user
// presses a key during that time which then is not accounted for in the stats

DWORD inputhookinactivity() { return g_dwLastTick; }
void inputhookstats(int &_k, int &_l, int &_r, int &_s)
{
    _k += (key + 1) / 2;
    key = 0;
    _l += lmb;
    lmb = 0;
    _r += rmb;
    rmb = 0;
    _s += (scr + 119) / 120;
    scr = 0;
}

extern void CheckAway(DWORD lasttime, DWORD curtime);

LRESULT CALLBACK KeyboardTracker(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        DWORD curtime = GetTickCount();
        CheckAway(g_dwLastTick, curtime);

        g_dwLastTick = curtime;
        key++;
        // OutputDebugStringA("key\n");
    }
    return ::CallNextHookEx(g_hHkKeyboardLL, code, wParam, lParam);
}

LRESULT CALLBACK MouseTracker(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        DWORD curtime = GetTickCount();
        CheckAway(g_dwLastTick, curtime);

        MOUSEHOOKSTRUCTEX *pStruct = (MOUSEHOOKSTRUCTEX *)lParam;
        switch (wParam)
        {
            case WM_LBUTTONDOWN: lmb++; break;
            case WM_RBUTTONDOWN: rmb++; break;
            case WM_MOUSEWHEEL: scr += abs(*(((short *)&pStruct->mouseData) + 1)); break;
        }
        if (pStruct->pt.x != g_mouseLocX || pStruct->pt.y != g_mouseLocY)
        {
            g_mouseLocX = pStruct->pt.x;
            g_mouseLocY = pStruct->pt.y;
            g_dwLastTick = curtime;
            // OutputDebugStringA("mm\n");
        }
    }
    return ::CallNextHookEx(g_hHkMouseLL, code, wParam, lParam);
}

bool inputhooksetup()
{
    if (!g_hHkKeyboardLL) g_hHkKeyboardLL = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardTracker, NULL, 0);
    if (!g_hHkMouseLL) g_hHkMouseLL = SetWindowsHookEx(WH_MOUSE_LL, MouseTracker, NULL, 0);

    g_dwLastTick = GetTickCount();

    return g_hHkKeyboardLL && g_hHkMouseLL;
}

void inputhookcleanup()
{
    if (g_hHkKeyboardLL) UnhookWindowsHookEx(g_hHkKeyboardLL);
    if (g_hHkMouseLL) UnhookWindowsHookEx(g_hHkMouseLL);
    g_hHkKeyboardLL = NULL;
    g_hHkMouseLL = NULL;
}

void reinstall_hooks()
{
    DWORD lasttick = g_dwLastTick;

    inputhookcleanup();
    if (!inputhooksetup())
    {
        panic("PT: can't set keyboard/mouse hooks, please restart ProcrastiTracker");
    }

    g_dwLastTick = lasttick;
}

// run the above functions on their own thread:

DWORD WINAPI hookproc(LPVOID)
{
    inputhooksetup();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (msg.hwnd)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
            switch (msg.message)
            {
                case WM_APP + 1:
                    // OutputDebugStringA("restarting hooks!\n");
                    reinstall_hooks();
                    break;
            }
    }

    inputhookcleanup();

    return 0;
}

HANDLE thandle = NULL;
DWORD tid = 0;

void launchhookthread()
{
    thandle = CreateThread(NULL, 0, hookproc, NULL, 0, &tid);
    if (!thandle) panic("can't launch hook thread!");
}

void killhookthread()
{
    if (PostThreadMessage(tid, WM_QUIT, 0, 0))  // if this fails, don't wait for thread
    {
        WaitForSingleObject(thandle,
                            1000);  // if it doesn't manage to quit, leave it, and we'll exit the process anyway
    }
}

void threadrestarthook() { PostThreadMessage(tid, WM_APP + 1, 0, 0); }
// alternative to hooking: raw input. doesn't work because it only works when own windows are active

/*
bool setuprawinput()
{
    RAWINPUTDEVICE Rid[2];

    Rid[0].usUsagePage = 0x01;
    Rid[0].usUsage = 0x02;
    Rid[0].dwFlags = 0;
    Rid[0].hwndTarget = 0;

    Rid[1].usUsagePage = 0x01;
    Rid[1].usUsage = 0x06;
    Rid[1].dwFlags = 0;
    Rid[1].hwndTarget = 0;

    return (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) != FALSE);
}

bool processrawinput(LPARAM lParam)
{
    UINT dwSize;

    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
    LPBYTE lpb = new BYTE[dwSize];
    if (lpb == NULL) return false;

    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) return false;

    RAWINPUT *raw = (RAWINPUT *)lpb;

    char szTempOutput[1024];

    if (raw->header.dwType == RIM_TYPEKEYBOARD)
    {
        sprintf(szTempOutput, " Kbd: make=%04x Flags:%04x Reserved:%04x ExtraInformation:%08x, msg=%04x VK=%04x\n",
                raw->data.keyboard.MakeCode, raw->data.keyboard.Flags, raw->data.keyboard.Reserved,
                raw->data.keyboard.ExtraInformation, raw->data.keyboard.Message, raw->data.keyboard.VKey);
        OutputDebugStringA(szTempOutput);
    }
    else if (raw->header.dwType == RIM_TYPEMOUSE)
    {
        sprintf(szTempOutput,
                "Mouse: usFlags=%04x ulButtons=%04x usButtonFlags=%04x usButtonData=%04xulRawButtons=%04x lLastX=%04x "
                "lLastY=%04x ulExtraInformation=%04x\r\n",
                raw->data.mouse.usFlags, raw->data.mouse.ulButtons, raw->data.mouse.usButtonFlags,
                raw->data.mouse.usButtonData, raw->data.mouse.ulRawButtons, raw->data.mouse.lLastX,
                raw->data.mouse.lLastY, raw->data.mouse.ulExtraInformation);
        OutputDebugStringA(szTempOutput);
    }

    delete[] lpb;
    return true;
}
*/
