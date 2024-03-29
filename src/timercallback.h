
BOOL CALLBACK EnumChildWindowsCallback(HWND hWnd, LPARAM lp) {
    DWORD *procids = (DWORD *)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);
    if (pid != procids[0]) procids[1] = pid;
    return TRUE;
}

VOID CALLBACK timerfunc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    if (changesmade &&
        dwTime - lastsavetime >
            prefs[PREF_AUTOSAVE].ival * 60 * 1000) {  // dwTime wrapping has NO effect on this!
        save();
        changesmade = false;
        threadrestarthook();
    }

    HWND h = GetForegroundWindow();
    if (!h)
        return;

    // Fix to full-screen foreground windows triggering idle
    // For example: video games played with a controller, movies / presentations / videos played fullscreen, etc
    // Checks if foreground window is the same size as primary monitor
    bool foregroundfullscreen = false;
    HWND hdesktop = GetDesktopWindow();
    if (!(h == hdesktop || h == GetShellWindow())) { // foreground isn't desktop or shell
        RECT fgrect, deskrect;
        GetWindowRect(h, &fgrect);
        GetWindowRect(hdesktop, &deskrect);
        if (fgrect.bottom == deskrect.bottom
            && fgrect.top == deskrect.top
            && fgrect.left == deskrect.left
            && fgrect.right == deskrect.right) {
            foregroundfullscreen = true;
        }
    }

    bool controllerconnected = iscontrollerconnected();
    DWORD last_activity = inputhookinactivity();
    if (controllerconnected && prefs[PREF_XINPUTACTIVITY].ival) {
        start_xinput_activity_timer();
        if (g_dwLastXInputTick > last_activity) {
            last_activity = g_dwLastXInputTick;
        }
    } else {
        stop_xinput_activity_timer();
    }
    
    DWORD idletime = (dwTime - last_activity) / 1000;  // same here
    if (foregroundfullscreen && prefs[PREF_FOREGROUNDFULLSCREEN].ival) {
        // break idle if conditions are met and user prefs want it
        idletime = 0; 
    }
    if (idletime > prefs[PREF_IDLE].ival) {
        if (!changesmade)
            // save one last time while idle, don't keep saving db while
            // idle, and don't immediately save on resume
            lastsavetime = dwTime;
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    changesmade = true;
    char exename[MAXTMPSTR];
    *exename = 0;
    char title[MAXTMPSTR];
    *title = 0;
    char url[MAXTMPSTR];
    *url = 0;
    
    DWORD procid = 0;
    GetWindowThreadProcessId(h, &procid);
    for (int tries = 0; tries < 2; tries++) {
        HANDLE ph = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ /*|PROCESS_SET_QUOTA*/,
                                FALSE, procid);

        if (!ph)
            return;

        DWORD count;
        HMODULE hm;
        if (qfpin) {  // we're on Vista or above, only this function allows a 32bit process to
                      // query a 64bit one
            wchar_t uexename[MAXTMPSTR];
            DWORD testl = sizeof(uexename);
            qfpin(ph, 0, uexename, &testl);
            uexename[MAXTMPSTR - 1] = 0;
            WideCharToMultiByte(CP_UTF8, 0, uexename, -1, exename, MAXTMPSTR, NULL, NULL);
            exename[MAXTMPSTR - 1] = 0;
        } else {  // we're on XP or 2000
            if (EnumProcessModules(ph, &hm, sizeof(HMODULE), &count))
                GetModuleFileNameExA(ph, hm, exename, MAXTMPSTR);
        }
        exename[MAXTMPSTR - 1] = 0;
        char *trim = strrchr(exename, '\\');
        if (trim) memmove(exename, trim + 1, strlen(trim));
        char *ext = strrchr(exename, '.');
        if (ext) *ext = 0;
        for (char *p = exename; *p; p++) *p = tolower(*p);
        CloseHandle(ph);

        // Windows 8/10 hide the real process for UWP apps in a wrapper process called WWAHost.exe or
        // ApplicationFrameHost.exe.
        if (strcmp(exename, "applicationframehost") != 0 && strcmp(exename, "wwahost") != 0) {
            break;
        }
        // so look for child windows with a different id, then use that instead. From:
        // http://stackoverflow.com/questions/32360149/name-of-process-for-active-window-in-windows-8-10
        DWORD procids[] = { procid, procid };
        EnumChildWindows(h, EnumChildWindowsCallback, (LPARAM)procids);
        procid = procids[1];
    }

    if (strcmp(exename, "lockapp") == 0) {
        return;
    }

    if (strcmp(exename, "firefox") == 0 || strcmp(exename, "iexplore") == 0 ||
        strcmp(exename, "chrome") == 0 || strcmp(exename, "opera") == 0 ||
        strcmp(exename, "netscape") == 0 || strcmp(exename, "netscape6") == 0 ||
        strcmp(exename, "msedge") == 0) {
        // TODO: can we get a UTF-8 URL out of this somehow, if the URL contains percent encoded
        // unicode chars?
        ddereq(exename, "WWW_GetWindowInfo", "0xFFFFFFFF", url, MAXTMPSTR);
        if (!*url) {
            if (!strcmp(exename, "chrome") || !strcmp(exename, "msedge")) {
                // Chrome doesn't support DDE, get last url change from it:
                strncpy(url, current_chrome_url, MAXTMPSTR);
            }

            // FIXME: Edge doesn't support DDE either, but currently no known workaround.

            // FIXME: And Firefox stopped supporting it as well:
            // https://bugzilla.mozilla.org/show_bug.cgi?id=1373212
            // It may be possible to use the "UI Automation" API to retrieve it?
            // https://stackoverflow.com/questions/20798935/how-can-i-get-the-url-from-running-instance-of-firefox-and-chrome-web-browser-in
            // Example of usage here:
            // https://code.msdn.microsoft.com/Windows-7-UI-Automation-9131f729
            // Also see WinEventProc in ddeutil.h
        }
        char *http = strstr(url, "://");
        if (!http) {
            http = url;
        } else {
            http += 3;
        }
        if (strncmp(http, "www.", 4) == 0) http += 4;
        size_t len = strcspn(http, "/\":@\0");
        http[len] = 0;
        memmove(url, http, len + 1);
    }

    wchar_t utitle[MAXTMPSTR];
    GetWindowTextW(h, utitle, MAXTMPSTR);
    utitle[MAXTMPSTR - 1] = 0;
    WideCharToMultiByte(CP_UTF8, 0, utitle, -1, title, MAXTMPSTR, NULL, NULL);
    title[MAXTMPSTR - 1] = 0;

    std::string s = exename;
    if (url[0] && s[0]) s += " - ";
    s += url;
    if (title[0] && s[0]) s += " - ";
    s += title;

    //OutputDebugF("timercallback: %s\n", s.c_str());

    if (s[0]) addtodatabase((char *)s.c_str(), st, idletime, 0);
};
