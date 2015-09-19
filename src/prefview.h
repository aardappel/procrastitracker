

INT_PTR CALLBACK Prefs(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
        {
            loop(i, NUM_PREFS) prefs[i].seteditbox(hDlg);
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
                default:
                {
                    loop(i, NUM_PREFS) prefs[i].handleeditbox(LOWORD(wParam));
                    return (INT_PTR)TRUE;
                }
            }
            break;
        }
    }
    return (INT_PTR)FALSE;
}
