

VOID CALLBACK timerfunc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    if (changesmade &&
        dwTime - lastsavetime > prefs[PREF_AUTOSAVE].ival * 60 * 1000)  // dwTime wrapping has NO effect on this!
    {
        // inputhookcleanup();

        save();
        changesmade = false;

        // reinstall_hooks();

        threadrestarthook();
    }

    DWORD idletime = (dwTime - inputhookinactivity()) / 1000;  // same here
    if (idletime > prefs[PREF_IDLE].ival)
    {
        if (!changesmade)
            lastsavetime = dwTime;  // save one last time while idle, don't keep saving db while idle, and don't
                                    // immediately save on resume
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);

    changesmade = true;

    const int MAXSTR = 1000;

    char exename[MAXSTR];
    *exename = 0;

    char title[MAXSTR];
    *title = 0;

    char url[MAXSTR];
    *url = 0;

    HWND h = GetForegroundWindow();
    if (h)
    {
        DWORD procid = 0;
        GetWindowThreadProcessId(h, &procid);

        HANDLE ph = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ /*|PROCESS_SET_QUOTA*/, FALSE, procid);
        if (ph)
        {
            // SIZE_T min = 0, max = 0;
            // GetProcessWorkingSetSize(ph, &min, &max);
            // SetProcessWorkingSetSize(ph, min, 1000000000);

            DWORD count;
            HMODULE hm;
            if (qfpin)  // we're on vista, only this function allows a 32bit process to query a 64bit one
            {
                wchar_t uexename[MAXSTR];
                DWORD testl = sizeof(uexename);
                qfpin(ph, 0, uexename, &testl);
                uexename[MAXSTR - 1] = 0;
                WideCharToMultiByte(CP_UTF8, 0, uexename, -1, exename, MAXSTR, NULL, NULL);
                exename[MAXSTR - 1] = 0;
            }
            else  // we're on XP or 2000
            {
                if (EnumProcessModules(ph, &hm, sizeof(HMODULE), &count)) GetModuleFileNameExA(ph, hm, exename, MAXSTR);
            }
            exename[MAXSTR - 1] = 0;
            char *trim = strrchr(exename, '\\');
            if (trim) memmove(exename, trim + 1, strlen(trim));
            char *ext = strrchr(exename, '.');
            if (ext) *ext = 0;
            for (char *p = exename; *p; p++) *p = tolower(*p);
            CloseHandle(ph);
        }

        if (strcmp(exename, "firefox") == 0 || strcmp(exename, "iexplore") == 0 || strcmp(exename, "chrome") == 0 ||
            strcmp(exename, "opera") == 0 || strcmp(exename, "netscape") == 0 || strcmp(exename, "netscape6") == 0)
        {
            // TODO: can we get a UTF-8 URL out of this somehow, if the URL contains percent encoded unicode chars?
            ddereq(exename, "WWW_GetWindowInfo", "0xFFFFFFFF", url, MAXSTR);
            url[MAXSTR - 1] = 0;
            char *http = strstr(url, "://");
            if (http)
            {
                http += 3;
                if (strncmp(http, "www.", 4) == 0) http += 4;
                size_t len = strcspn(http, "/\":@");
                http[len] = 0;
                memmove(url, http, len + 1);
            }
            else
            {
                *url = 0;
            }
        }

        wchar_t utitle[MAXSTR];
        GetWindowTextW(h, utitle, MAXSTR);
        utitle[MAXSTR - 1] = 0;
        WideCharToMultiByte(CP_UTF8, 0, utitle, -1, title, MAXSTR, NULL, NULL);
        title[MAXSTR - 1] = 0;
    }

    std::string s = exename[0] ? exename : "(null)";
    if (url[0]) s += " - ";
    s + url;
    if (title[0]) s += " - ";
    s += title;

    // char buf[MAXSTR];
    // sprintf_s(buf, MAXSTR, "%s%s%s%s%s", exename[0] ? exename : "(null)", url[0] ? " - " : "", url, title[0] ? " - "
    // : "", title);
    // buf[MAXSTR-1] = 0;

    addtodatabase((char *)s.c_str(), st, idletime, 0);
};
