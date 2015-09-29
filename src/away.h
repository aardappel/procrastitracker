
INT_PTR CALLBACK Away(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
        {
            HWND text = GetDlgItem(hDlg, IDC_STATIC);
            assert(text);
            char buf[100];
            sprintf_s(buf, 100, "You were away for %d minutes, what was your primary activity?", awaysecsdialog / 60);
            SetWindowTextA(text, buf);
            return (INT_PTR)TRUE;
        }

        case WM_CLOSE:
        case WM_DESTROY: awaydialog = NULL; break;

        case WM_COMMAND:
            if (LOWORD(wParam) != IDC_BUTTON8)
            {
                HWND button = GetDlgItem(hDlg, LOWORD(wParam));
                if (button)
                {
                    char buf[200] = "Away : ";
                    getcontroltext(button, buf + strlen(buf), 100);
                    SYSTEMTIME st;
                    GetLocalTime(&st);
                    addtodatabase(buf, st, 0, awaysecsdialog);
                }
            }
            EndDialog(hDlg, LOWORD(wParam));
            awaydialog = NULL;
            return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

// this one runs on the main thread
void CreateAwayDialog(DWORD awaysecs)
{
    if (awaydialog) EndDialog(awaydialog, IDC_BUTTON8);  // if the user hadn't responded to the previous dialog, kill it
    awaysecsdialog = awaysecs;
    awaydialog = CreateDialog(hInst, MAKEINTRESOURCE(IDD_AWAYDIALOG), mainhwnd, Away);
    if (!awaydialog)
    {
        DWORD err = GetLastError();
        printf("CreateDialog failed: %d\n", err);
    }
}

// this one runs on the hook thread
void CheckAway(DWORD lasttime, DWORD curtime)
{
    DWORD awaysecs = (curtime - lasttime) / 1000;
    DWORD awaymins = awaysecs / 60;

    // accessing prefs here is theoretically also thread-unsafe
    if (awaysecs > prefs[PREF_IDLE].ival &&  // we're waking up from an idle period
        awaymins < prefs[PREF_AWAY].ival)    // we actually want to track this (i.e. not a night)
    {
        // if (!PostThreadMessage(mainthreadid, WM_APP + 1, 0, awaysecs))
        if (!PostMessage(mainhwnd, WM_APP + 1, 0, awaysecs)) OutputDebugStringA("posting WM_APP failed\n");
    }
}
