
DWORD idInst = 0;
HDDEDATA CALLBACK DdeCallback(UINT uType, UINT uFmt, HCONV hconv, HSZ hsz1, HSZ hsz2,
                              HDDEDATA hdata, DWORD dwData1, DWORD dwData2) {
    return 0;
}
bool ddeinit() {
    return DdeInitialize(&idInst, (PFNCALLBACK)DdeCallback, APPCLASS_STANDARD | APPCMD_CLIENTONLY,
                         0) == DMLERR_NO_ERROR;
}
void ddeclean() {
    if (idInst) DdeUninitialize(idInst);
}

void ddereq(char *server, char *topic, char *item, char *buf, int len) {
    buf[0] = 0;
    HSZ hszApp = DdeCreateStringHandleA(idInst, server, 0);
    HSZ hszTopic = DdeCreateStringHandleA(idInst, topic, 0);
    HCONV hConv = DdeConnect(idInst, hszApp, hszTopic, NULL);
    DdeFreeStringHandle(idInst, hszApp);
    DdeFreeStringHandle(idInst, hszTopic);
    if (hConv == NULL) {
        // OutputDebugF("dde error: %x\n", DdeGetLastError(idInst));
        // DMLERR_NO_CONV_ESTABLISHED on chrome, see
        // https://bugs.chromium.org/p/chromium/issues/detail?id=70184
        return;
    }

    HSZ hszItem = DdeCreateStringHandleA(idInst, item, 0);
    HDDEDATA hData =
        DdeClientTransaction(NULL, 0, hConv, hszItem, CF_TEXT, XTYP_REQUEST, 5000, NULL);
    if (hData != NULL) {
        DdeGetData(hData, (unsigned char *)buf, len, 0);
        buf[len - 1] = 0;
        DdeFreeDataHandle(hData);
    } else {
        // OutputDebugF("dde error: %x\n", DdeGetLastError(idInst));
    }

    DdeFreeStringHandle(idInst, hszItem);

    DdeDisconnect(hConv);
}

// Chrome still doesn't support DDE, so we use this kludgey code instead to get the current URL.
// https://bugs.chromium.org/p/chromium/issues/detail?id=70184
// http://stackoverflow.com/questions/21010017/how-to-get-current-url-for-chrome-current-version

// Firefox:
// https://wiki.mozilla.org/Accessibility/AT-Windows-API

HWINEVENTHOOK LHook = 0;
char current_chrome_url[MAXTMPSTR] = {0};

#ifdef MINGW32_BUG
void WinEventProc
#else
void CALLBACK WinEventProc
#endif
    (HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
     DWORD dwEventThread, DWORD dwmsEventTime) {
    char classname[MAXTMPSTR];
    GetClassName(hwnd, classname, MAXTMPSTR);
    classname[MAXTMPSTR - 1] = 0;
    auto is_chrome = strcmp(classname, "Chrome_WidgetWin_1") != 0;
    auto is_firefox = strcmp(classname, "MozillaWindowClass") != 0;
    if (!is_chrome /* && !is_firefox */) return;  // Early out.
    IAccessible *pAcc = NULL;
    VARIANT varChild;
    HRESULT hr = AccessibleObjectFromEvent(hwnd, idObject, idChild, &pAcc, &varChild);
    if ((hr == S_OK) && (pAcc != NULL)) {
        BSTR bstrName, bstrValue;
        pAcc->get_accValue(varChild, &bstrValue);
        pAcc->get_accName(varChild, &bstrName);
        #ifdef TEST_FIREFOX
            OutputDebugStringW(bstrName);
            OutputDebugStringA(" = ");
            OutputDebugStringW(bstrValue);
            OutputDebugStringA("\n");
        #endif
        if (bstrName && bstrValue && !wcscmp(bstrName, L"Address and search bar")) {
            WideCharToMultiByte(CP_UTF8, 0, bstrValue, -1, current_chrome_url, MAXTMPSTR, NULL,
                                NULL);
            current_chrome_url[MAXTMPSTR - 1] = 0;
        }
        pAcc->Release();
    }
}

void eventhookinit() {
    if (LHook != 0) return;
    CoInitialize(NULL);
    LHook = SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_VALUECHANGE, 0, WinEventProc, 0, 0,
                            WINEVENT_SKIPOWNPROCESS);
}

void eventhookclean() {
    if (LHook == 0) return;
    UnhookWinEvent(LHook);
    CoUninitialize();
}
