/*
INT_PTR CALLBACK Register(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    verifykey();

    switch (message)
    {
        case WM_INITDIALOG:
        {
            keyedit = seteditbox(hDlg, IDC_EDIT1, curkey);
            seteditbox(hDlg, IDC_EDIT2, keystatustext());
            return (INT_PTR)TRUE;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDCANCEL:
                {
                    EndDialog(hDlg, LOWORD(wParam));
                    return (INT_PTR)TRUE;
                }
                case IDC_EDIT1:
                {
                    getcontroltext(keyedit, curkey, KEYSIZE);
                    verifykey();
                    seteditbox(hDlg, IDC_EDIT2, keystatustext());
                    return (INT_PTR)TRUE;
                }
                default:
                {
                    // return (INT_PTR)TRUE;
                }
            }
            break;
        }
    }
    return (INT_PTR)FALSE;
}
*/
