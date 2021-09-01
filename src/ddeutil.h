
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
// Newer: https://stackoverflow.com/questions/48504300/get-active-tab-url-in-chrome-with-c

// Firefox:
// https://wiki.mozilla.org/Accessibility/AT-Windows-API

HWINEVENTHOOK LHook = 0;
char current_chrome_url[MAXTMPSTR] = {0};

#define DEBUG_URL 0

void DebugURL(const char* msg) {
    #if DEBUG_URL
    OutputDebugStringA(msg);
    #endif
}

void DebugURL(LPCWSTR msg) {
    #if DEBUG_URL
        OutputDebugStringW(msg);
    #endif
}

#ifdef MINGW32_BUG
void WinEventProc
#else
void CALLBACK WinEventProc
#endif
    (HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
     DWORD dwEventThread, DWORD dwmsEventTime) {
    if (!hwnd) {
        DebugURL("NULL HWND\n");
        return;
    }
    char classname[MAXTMPSTR];
    if (!GetClassName(hwnd, classname, MAXTMPSTR)) {
        DebugURL("GetClassName FAIL\n");
        return;
    }
    classname[MAXTMPSTR - 1] = 0;
    DebugURL("GetClassName: ");
    DebugURL(classname);
    DebugURL("\n");

    // Getting "HwndWrapper[DefaultDomain;;$UID]" now?
    auto is_chrome = strcmp(classname, "Chrome_WidgetWin_1") == 0;  // This can also be MS Edge
    //auto is_firefox = strcmp(classname, "MozillaWindowClass") == 0;
    if (!is_chrome /* && !is_firefox */) {
        DebugURL("NOT CHROME/EDGE\n");
        return;  // Early out.
    }

    #if 1

    IAccessible *pAcc = NULL;
    VARIANT varChild;
    HRESULT hr = AccessibleObjectFromEvent(hwnd, idObject, idChild, &pAcc, &varChild);
    if ((hr == S_OK) && (pAcc != NULL)) {
        BSTR bstrName, bstrValue;
        // FIXME: These calls, on some system, can cause message pump blocking?? which blocks
        // the entire app.
        pAcc->get_accValue(varChild, &bstrValue);
        pAcc->get_accName(varChild, &bstrName);
        DebugURL(bstrName);
        DebugURL(" = ");
        DebugURL(bstrValue);
        DebugURL("\n");
        if (bstrName && bstrValue && !wcscmp(bstrName, L"Address and search bar")) {
            WideCharToMultiByte(CP_UTF8, 0, bstrValue, -1, current_chrome_url, MAXTMPSTR, NULL,
                NULL);
            current_chrome_url[MAXTMPSTR - 1] = 0;
            DebugURL("Got URL:");
            DebugURL(current_chrome_url);
            DebugURL("\n");
        } else {
            DebugURL("Address and search bar fail\n");
        }
        pAcc->Release();
    } else {
        DebugURL("AccessibleObjectFromEvent fail\n");
    }

    #else

    // New way: https://stackoverflow.com/questions/48504300/get-active-tab-url-in-chrome-with-c
    // This doesn't work .. the URL is always empty.
    // And spams OLEAUT "Library not registered" errors.

    for (;;) {
        CComQIPtr<IUIAutomation> uia;
        if (FAILED(uia.CoCreateInstance(CLSID_CUIAutomation)) || !uia) {
            DebugURL("CoCreateInstance fail\n");
            break;
        }

        CComPtr<IUIAutomationElement> root;
        if (FAILED(uia->ElementFromHandle(hwnd, &root)) || !root) {
            DebugURL("ElementFromHandle fail\n");
            break;
        }

        CComPtr<IUIAutomationCondition> condition;

        //URL's id is 0xC354, or use UIA_EditControlTypeId for 1st edit box
        if (FAILED(uia->CreatePropertyCondition(UIA_ControlTypePropertyId,
            CComVariant(0xC354), &condition))) {
            DebugURL("CreatePropertyCondition fail\n");
            break;
        }

        //or use edit control's name instead
        //uia->CreatePropertyCondition(UIA_NamePropertyId,
        //      CComVariant(L"Address and search bar"), &condition);

        CComPtr<IUIAutomationElement> edit;
        if (FAILED(root->FindFirst(TreeScope_Descendants, condition, &edit))
            || !edit) {
            DebugURL("FindFirst fail\n");
            break;
            continue; //maybe we don't have the right tab, continue...
        }

        CComVariant url;
        if (FAILED(edit->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &url))) {
            DebugURL("GetCurrentPropertyValue fail\n");
            break;
        }

        WideCharToMultiByte(CP_UTF8, 0, url.bstrVal, -1, current_chrome_url, MAXTMPSTR, NULL,
            NULL);
        current_chrome_url[MAXTMPSTR - 1] = 0;
        DebugURL("Got URL:");
        DebugURL(current_chrome_url);
        DebugURL("\n");

        break;
    }

    #endif
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
