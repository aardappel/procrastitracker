
HWND seteditbox(HWND w, int itemconst, char *aval) {
    HWND h = GetDlgItem(w, itemconst);
    SetWindowTextA(h, aval);
    return h;
}

void getcontroltext(HWND h, char *aval, int len) {
    GetWindowTextA(h, aval, len);
    aval[len - 1] = 0;
}

struct numeditbox {
    HWND h;
    DWORD ival;
    DWORD min, max;
    int itemconst;
    bool check = false;

    void seteditbox(HWND w) {
        if (check) {
            h = w; 
            CheckDlgButton(w, itemconst, ival == 0 ? BST_UNCHECKED : BST_CHECKED);
        }
        else {
            char aval[100];
            sprintf(aval, "%d", ival);
            h = ::seteditbox(w, itemconst, aval);
        }
    }
    bool handleeditbox(int item) {
        char aval[100] = "";
        if (item != itemconst) return false;
        int val = ival;
        if (check) {
            ival = IsDlgButtonChecked(h, itemconst) == BST_CHECKED;
        } else {
            getcontroltext(h, aval, sizeof(aval));
            if (!aval[0])
                return false;  // this shouldn't be needed? why is this called upon window init?
            ival = atoi(aval);
        }
        if (ival == val) return false;
        if (ival < min) ival = min;
        if (ival > max) ival = max;
        return ival != val;
    }
};

void comboboxitem(HWND cb, char *name, int item, int image, UINT msg) {
    COMBOBOXEXITEMA cbi;
    cbi.mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
    cbi.pszText = name;
    cbi.iItem = item;
    cbi.iImage = image;
    cbi.iSelectedImage = image;
    SendMessageA(cb, msg, 0, (LPARAM)&cbi);
}

void listitem(HWND cb, char *name, int item, int image, UINT msg) {
    LVITEMA lvi;
    lvi.mask = LVIF_TEXT | LVIF_IMAGE;
    lvi.pszText = name;
    lvi.iItem = item;
    lvi.iSubItem = 0;
    lvi.iImage = image;
    SendMessageA(cb, msg, 0, (LPARAM)&lvi);
}
