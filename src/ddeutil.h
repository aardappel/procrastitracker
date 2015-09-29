
DWORD idInst = 0;
HDDEDATA CALLBACK DdeCallback(UINT uType, UINT uFmt, HCONV hconv, HSZ hsz1, HSZ hsz2, HDDEDATA hdata, DWORD dwData1,
                              DWORD dwData2)
{
    return 0;
}
bool ddeinit()
{
    return DdeInitialize(&idInst, (PFNCALLBACK)DdeCallback, APPCLASS_STANDARD | APPCMD_CLIENTONLY, 0) ==
           DMLERR_NO_ERROR;
}
void ddeclean()
{
    if (idInst) DdeUninitialize(idInst);
}

void ddereq(char *server, char *topic, char *item, char *buf, int len)
{
    HSZ hszApp = DdeCreateStringHandleA(idInst, server, 0);
    HSZ hszTopic = DdeCreateStringHandleA(idInst, topic, 0);
    HCONV hConv = DdeConnect(idInst, hszApp, hszTopic, NULL);
    DdeFreeStringHandle(idInst, hszApp);
    DdeFreeStringHandle(idInst, hszTopic);

    if (hConv == NULL) return;

    HSZ hszItem = DdeCreateStringHandleA(idInst, item, 0);
    HDDEDATA hData = DdeClientTransaction(NULL, 0, hConv, hszItem, CF_TEXT, XTYP_REQUEST, 5000, NULL);
    if (hData != NULL)
    {
        DdeGetData(hData, (unsigned char *)buf, len, 0);
        DdeFreeDataHandle(hData);
    }
    DdeFreeStringHandle(idInst, hszItem);

    DdeDisconnect(hConv);
}
