
void setbar(int tag, HDC hdc)
{
    HBITMAP &bm = tags[tag].barbm;
    if (!bm)
    {
        if (!bitmapdc) bitmapdc = CreateCompatibleDC(hdc);
        bm = CreateCompatibleBitmap(hdc, 1920, 64);
        HBITMAP old = (HBITMAP)SelectObject(bitmapdc, bm);
        RECT r = {0, 0, 1600, 64};  // FIXME, screensize
        FillRect(bitmapdc, &r, CreateSolidBrush(tags[tag].color));
        SelectObject(bitmapdc, old);
    }
    else
    {
        SelectObject(bitmapdc, bm);
    }
}

const int controlmargin = 6;

RECT graphrect;
int graphh = 1;
bool isongraph = true;

void renderdaystat(HDC hdc, HWND hDlg)
{
    int numdays = 0;
    DWORD biggesttotal = 0;
    loopv(i, daystats)
    {
        DWORD sec = daystats[i].total;
        if (sec)
        {
            numdays++;
            if (sec > biggesttotal) biggesttotal = sec;
        }
    }

    if (!numdays) return;

    RECT r;
    GetClientRect(hDlg, &r);
    r.left = r.right - daygraphwidth;
    r.top += controlmargin;
    r.left += controlmargin;
    r.bottom -= controlmargin;
    r.right -= controlmargin;
    FillRect(hdc, &r, greybrush);
    r.top += 1;
    r.left += 1;
    r.bottom -= 1;
    r.right -= 1;
    FillRect(hdc, &r, whitebrush);

    // GetWindowRect(daystatctrl, &r);
    int barh = (r.bottom - r.top) / numdays;
    if (!barh)
    {
        barh = 1;
        numdays = r.bottom - r.top;
    }
    if (barh > 20) barh = 20;
    int barw = r.right - r.left;

    graphrect = r;
    graphh = barh;

    int day = 0;

    loop(i, numdays)
    {
        while (!daystats[day].total) day++;
        int bstart = r.left;
        RECT rt = {0, r.top + i * barh, 0, r.top + (i + 1) * barh};
        loop(j, MAXTAGS)
        {
            int sz = daystats[day].seconds[j] * barw / biggesttotal;
            if (sz)
            {
                rt.left = bstart;
                rt.right = (bstart += sz);
                if (!tags[j].br) tags[j].br = CreateSolidBrush(tags[j].color);
                FillRect(hdc, &rt, tags[j].br);
            }
        }
        day++;
    }
}

void redrawtreeview()
{
    RECT r;
    GetClientRect(treeview, &r);
    InvalidateRect(treeview, &r, TRUE);
}

void recompaccum()
{
    root->checkstrfilter(false);

    daydata d;
    tagstat ts;

    daystats.setsize(0);
    loop(i, endtime - starttime + 1) daystats.push(tagstat());

    root->accumulate(d, ts);
}

void rendertree(HWND hDlg, bool graphalso)
{
    recompaccum();

    TreeView_DeleteAllItems(treeview);

    root->treeview(0, hDlg, NULL, TVI_ROOT);

    loopv(i, daystats) daystats[i].sum();
    // renderdaystat(GetDC(daystatctrl));

    if (graphalso)
    {
        RECT r;
        GetClientRect(hDlg, &r);
        r.left = r.right - daygraphwidth;
        InvalidateRect(hDlg, &r, TRUE);
    }
}

long handleCustomDraw(HWND hWndTreeView, LPNMTVCUSTOMDRAW pNMTVCD)
{
    if (pNMTVCD == NULL) return -1;
    switch (pNMTVCD->nmcd.dwDrawStage)
    {
        case CDDS_PREPAINT: return CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT: return CDRF_NOTIFYPOSTPAINT;

        case CDDS_ITEMPOSTPAINT:
        {
            RECT rc;
            TreeView_GetItemRect(hWndTreeView, (HTREEITEM)pNMTVCD->nmcd.dwItemSpec, &rc, 1);

            HTREEITEM hItem = (HTREEITEM)pNMTVCD->nmcd.dwItemSpec;
            TVITEM tvi = {0};
            tvi.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
            tvi.hItem = hItem;
            tvi.stateMask = -1;

            TreeView_GetItem(hWndTreeView, &tvi);
            if (tvi.lParam)
            {
                if (tvi.state & TVIS_EXPANDED && TreeView_GetChild(hWndTreeView, hItem))
                {
                    rc.left = rc.right;
                    rc.right = bargraphwidth;
                    FillRect(pNMTVCD->nmcd.hdc, &rc, whitebrush);
                }
                else
                {
                    node *n = (node *)tvi.lParam;
                    tagstat ts;
                    if (n->ts)
                        ts = *n->ts;
                    else
                        ts.seconds[n->gettag()] = n->accum.seconds;

                    int totalpixels = n->accum.seconds / (float)maxsecondsforbargraph * (bargraphwidth - rc.left);

                    loop(i, MAXTAGS) if (ts.seconds[i])
                    {
                        setbar(i, pNMTVCD->nmcd.hdc);
                        int segwidth = totalpixels / (float)n->accum.seconds * ts.seconds[i];
                        rc.right = rc.left + segwidth;
                        BitBlt(pNMTVCD->nmcd.hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, bitmapdc, 0,
                               0, SRCAND);
                        rc.left += segwidth;
                    }
                }
            }
            return CDRF_DODEFAULT;
        }
        break;
    }
    return 0;
}

long handleNotify(HWND hWndDlg, int nIDCtrl, LPNMHDR pNMHDR)
{
    switch (pNMHDR->code)
    {
        case NM_CUSTOMDRAW:
        {
            if (nIDCtrl == IDC_TREE1)
            {
                LPNMTVCUSTOMDRAW pNMTVCD = (LPNMTVCUSTOMDRAW)pNMHDR;
                HWND hWndTreeView = pNMHDR->hwndFrom;

                return handleCustomDraw(hWndTreeView, pNMTVCD);
            }
            break;
        }
        /*
        case NM_RCLICK:
        {
            if(nIDCtrl == IDC_TREE1)
            {
                HWND hWndTreeView = pNMHDR->hwndFrom;


            }
            break;
        }
        */

        case TVN_KEYDOWN:
        {
            NMTVKEYDOWN *kd = (NMTVKEYDOWN *)pNMHDR;
            if (selectednode) switch (kd->wVKey)
                {
                    case 'C':
                    {
                        if (selectednode->last)
                        {
                            char buf[100] = "";
                            CWin32InputBox::InputBox("Manual Override",
                                                     "Enter new amount of minutes to add to this node", buf, 100, false,
                                                     hWndDlg);
                            int newv = (int)selectednode->last->seconds + atoi(buf) * 60;
                            selectednode->last->seconds = max(newv, 0);
                            rendertree(hWndDlg, false);
                        }
                        return TRUE;
                    }

                    case 'H':
                        selectednode->hidden = true;
                        rendertree(hWndDlg, false);
                        return TRUE;

                    case 'U':
                        selectednode->clearhidden();
                        rendertree(hWndDlg, false);
                        return TRUE;

                    case 'P':
                        selectednode->firstinchain()->mergallsubstring();
                        rendertree(hWndDlg, false);
                        return TRUE;

                    case 'M':
                        if (prevselectednode)
                        {
                            prevselectednode = prevselectednode->firstinchain();
                            selectednode = selectednode->firstinchain();
                            if (prevselectednode != selectednode && prevselectednode->parent)
                            {
                                selectednode->merge(*prevselectednode);
                                prevselectednode->parent->remove(prevselectednode);
                                selectednode = prevselectednode = NULL;
                                rendertree(hWndDlg, false);
                            }
                        }
                        return TRUE;
                }
            break;
        }
        /*
        case TVN_BEGINDRAG:
        {
            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pNMHDR;
            begindragnode = (node *)pnmtv->itemNew.lParam;
            break;
        }
        */

        case TVN_SELCHANGED:
        {
            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pNMHDR;
            prevselectednode = selectednode;
            selectednode = (node *)pnmtv->itemNew.lParam;
            // SendMessageA(tagdrop, CB_SETCURSEL, selectednode->tag, 0);
            break;
        }

        case TVN_ITEMEXPANDED:
        {
            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pNMHDR;
            node *enode = (node *)pnmtv->itemNew.lParam;
            enode->expanded = pnmtv->action != TVE_COLLAPSE;
            break;
        }

        case TVN_GETINFOTIP:
        {
            NMTVGETINFOTIP *it = (NMTVGETINFOTIP *)pNMHDR;

            TVITEM tvi = {0};
            tvi.mask = TVIF_HANDLE | TVIF_PARAM;
            tvi.hItem = it->hItem;
            TreeView_GetItem(treeview, &tvi);
            if (tvi.lParam)
            {
                node *n = (node *)tvi.lParam;
                String s;
                n->formatstats(s);
                strncpy(it->pszText, s.c_str(), it->cchTextMax);
            }
            else
            {
                strcpy(it->pszText, "test");
            }
            break;
        }

        /*
        case NM_CLICK:
        {
            break;
        }
        */

        case LVN_BEGINLABELEDIT:
        {
            taglistedit = ListView_GetEditControl(taglist);
            break;
        }

        case LVN_ENDLABELEDIT:
        {
            int sel = SendMessage(taglist, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
            if (sel < 0) break;
            getcontroltext(taglistedit, tags[sel].name, sizeof(tags[sel].name));
            listitem(taglist, tags[sel].name, sel, sel, LVM_SETITEMA);
            break;
        }

        /*
        case CBEN_ENDEDITA:
        {
        PNMCBEENDEDITA ee = (PNMCBEENDEDITA)pNMHDR;
        if(ee->iNewSelection!=CB_ERR)
        {
        if(selectednode) selectednode->tag = ee->iNewSelection;
        if(ee->fChanged)
        {
        strcpy_s(tags[ee->iNewSelection].name, sizeof(tags[0].name), ee->szText);
        tags[ee->iNewSelection].name[sizeof(tags[0].name)-1] = 0;
        }
        RECT r;
        GetWindowRect(treeview, &r);
        InvalidateRect(treeview, &r, TRUE);
        }
        break;
        }
        */

        case DTN_DATETIMECHANGE:
        {
            SYSTEMTIME start, end;
            SendMessageA(startingdatepicker, DTM_GETSYSTEMTIME, 0, (LPARAM)&start);
            SendMessageA(endingdatepicker, DTM_GETSYSTEMTIME, 0, (LPARAM)&end);
            starttime = dayordering(start);
            endtime = dayordering(end);
            rendertree(hWndDlg, true);
            break;
        }
    }
    return 0;
}

void setdaterangecontrols(HWND hDlg)
{
    daydata tempday;
    SYSTEMTIME st;

    tempday.nday = starttime;
    tempday.createsystime(st);
    startingdatepicker = GetDlgItem(hDlg, IDC_DATETIMEPICKER1);
    SendMessageA(startingdatepicker, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);

    tempday.nday = endtime;
    tempday.createsystime(st);
    endingdatepicker = GetDlgItem(hDlg, IDC_DATETIMEPICKER2);
    SendMessageA(endingdatepicker, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
}

INT_PTR CALLBACK Stats(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
        {
            filterontag = -1;
            selectednode = NULL;
            filterstrcontents[0] = 0;

            treeview = GetDlgItem(hDlg, IDC_TREE1);
            // daystatctrl = GetDlgItem(hDlg, IDC_LIST1);

            rendertree(hDlg, false);

            // tagdrop = GetDlgItem(hDlg, IDC_COMBOBOXEX1);
            taglist = GetDlgItem(hDlg, IDC_LIST1);

            if (!tagimages)
            {
                tagimages = ImageList_Create(bmsize, bmsize, ILC_COLORDDB, MAXTAGS, 0);
                HDC hdc = GetDC(NULL);
                HDC bitmapdc = CreateCompatibleDC(hdc);
                HBITMAP bitmap = CreateCompatibleBitmap(hdc, bmsize, bmsize * MAXTAGS);
                loop(i, MAXTAGS)
                {
                    HBITMAP old = (HBITMAP)SelectObject(bitmapdc, bitmap);
                    loop(x, bmsize) loop(y, bmsize) SetPixel(bitmapdc, x, y, tags[i].color);
                    SelectObject(bitmapdc, old);  // seems to be required to flush the drawing commands
                    ImageList_Add(tagimages, bitmap, NULL);
                }
                DeleteObject(bitmap);
                DeleteDC(bitmapdc);
                ReleaseDC(NULL, hdc);
            }

            // SendMessageA(tagdrop, CBEM_SETIMAGELIST, 0, (LPARAM)tagimages);
            // loop(i, MAXTAGS) comboboxitem(tagdrop, tags[i].name, i, i, CBEM_INSERTITEMA);

            // SendMessageA(taglist, LVM_SETIMAGELIST, 0, (LPARAM)tagimages);
            ListView_SetImageList(taglist, tagimages, LVSIL_SMALL);
            loop(i, MAXTAGS) listitem(taglist, tags[i].name, i, i, LVM_INSERTITEMA);

            foldslider = GetDlgItem(hDlg, IDC_SLIDER1);
            SendMessageA(foldslider, TBM_SETRANGE, FALSE, (LPARAM)MAKELONG(1, 5));
            SendMessageA(foldslider, TBM_SETPOS, TRUE, foldlevel);

            minfilter.seteditbox(hDlg);

            endtime = now();

            setdaterangecontrols(hDlg);

            quickcombo = GetDlgItem(hDlg, IDC_COMBO1);
            SendMessageA(quickcombo, CB_ADDSTRING, 0, (LPARAM) "Today");
            SendMessageA(quickcombo, CB_ADDSTRING, 0, (LPARAM) "Yesterday");
            SendMessageA(quickcombo, CB_ADDSTRING, 0, (LPARAM) "Since Monday Morning");
            SendMessageA(quickcombo, CB_ADDSTRING, 0, (LPARAM) "Last 7 Days");
            SendMessageA(quickcombo, CB_ADDSTRING, 0, (LPARAM) "Last 14 Days");
            SendMessageA(quickcombo, CB_ADDSTRING, 0, (LPARAM) "Month To Date");
            SendMessageA(quickcombo, CB_ADDSTRING, 0, (LPARAM) "30 Days");
            SendMessageA(quickcombo, CB_ADDSTRING, 0, (LPARAM) "90 Days");
            SendMessageA(quickcombo, CB_ADDSTRING, 0, (LPARAM) "Year To Date");
            SendMessageA(quickcombo, CB_ADDSTRING, 0, (LPARAM) "Since A Year Ago");
            SendMessageA(quickcombo, CB_ADDSTRING, 0, (LPARAM) "All Time");

            filterstr = GetDlgItem(hDlg, IDC_EDIT8);

            // SetFocus(GetDlgItem(hDlg, IDC_BUTTON8));
            // SendMessage(hDlg, DM_SETDEFID, (WPARAM)IDC_BUTTON8, 0);
            // SendMessage(hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg, IDC_BUTTON8), TRUE);

            return (INT_PTR)TRUE;
        }

        case WM_SIZING:
        {
            int min_x = 400;
            int min_y = 300;

            LPRECT r = (LPRECT)lParam;
            int ux = -1, uy = -1, wx = r->right - r->left, wy = r->bottom - r->top;
            if (wy < min_y) uy = min_y;
            if (wx < min_x) ux = min_x;

            if (uy != -1)
            {
                if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT)
                {
                    r->top = r->bottom - uy;
                }
                else
                {
                    r->bottom = r->top + uy;
                }
            }
            if (ux != -1)
            {
                if (wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT)
                {
                    r->left = r->right - ux;
                }
                else
                {
                    r->right = r->left + ux;
                }
            }
            return TRUE;
        }

        case WM_SIZE:
        {
            RECT r, rsl;  //, rds;
            GetClientRect(hDlg, &r);
            GetWindowRect(foldslider, &rsl);
            // GetWindowRect(daystatctrl, &rds);

            r.left += rsl.right - rsl.left + 12;
            r.right -= daygraphwidth;
            bargraphwidth = r.right - r.left;

            MoveWindow(treeview, r.left, r.top + controlmargin, r.right - r.left, r.bottom - r.top - controlmargin * 2,
                       TRUE);

            r.right += daygraphwidth;
            InvalidateRect(hDlg, &r, TRUE);

            // GetClientRect(hDlg, &r);
            // r.left = r.right-daygraphwidth;

            // MoveWindow(daystatctrl, r.left, r.top, r.right-r.left, r.bottom-r.top, TRUE);
            // InvalidateRect(hDlg, &r, TRUE);
            break;
        }

        case WM_MOUSEMOVE:
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            HWND sho = GetDlgItem(hDlg, IDC_STATICHO);
            if (x < graphrect.left || x >= graphrect.right || y < graphrect.top || y >= graphrect.bottom)
            {
                if (isongraph) SetWindowTextA(sho, "Hover over graph...");
                isongraph = false;
                /*
                if(begindragnode)
                {
                    TVHITTESTINFO ht;
                    ht.pt.x = x;
                    ht.pt.y = y;
                    HTREEITEM item = TreeView_HitTest(treeview, &ht);
                    if(item)
                    {
                        TVITEM tvi = { 0 };
                        tvi.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
                        tvi.hItem = item;
                        tvi.stateMask = -1;
                        TreeView_GetItem(treeview, &tvi);
                        if(tvi.lParam)
                        {
                            if((wParam&MK_LBUTTON) && (wParam&MK_CONTROL))
                            {
                                enddragnode = (node *)tvi.lParam;
                            }
                            else if(enddragnode)
                            {
                                if(begindragnode!=enddragnode && begindragnode->parent==enddragnode->parent)
                                {
                                    enddragnode->merge(begindragnode);
                                }
                                enddragnode = NULL;
                                begindragnode = NULL;
                            }
                        }
                    }
                }*/
            }
            else
            {
                int ypos = 0;
                loopv(i, daystats)
                {
                    DWORD sec = daystats[i].total;
                    if (sec)
                    {
                        int ih = y - graphrect.top - ypos * graphh;
                        if (ih >= 0 && ih < graphh)
                        {
                            daydata d;
                            d.nday = i + starttime;
                            SYSTEMTIME st;
                            d.createsystime(st);
                            d.seconds = sec;
                            String s;
                            s.Format("%d-%d-%d -> ", st.wYear, st.wMonth, st.wDay);
                            d.format(s, 0);
                            SetWindowTextA(sho, s);
                            isongraph = true;
                            break;
                        }
                        ypos++;
                    }
                }
            }
            break;
        }

        case WM_LBUTTONUP: { break;
        }

        case WM_MOUSEWHEEL: { break;
        }

        case WM_DRAWITEM: { break;
        }

        case WM_KEYUP: { break;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hDlg, &ps);
            renderdaystat(hdc, hDlg);
            /*
            HWND h = GetDlgItem(hDlg, IDC_BUTTON2);
            RECT r;
            GetClientRect(h, &r);
            HDC dc = GetDC(h);
            FillRect(dc, &r, CreateSolidBrush(0xFF00FF));
            */

            EndPaint(hDlg, &ps);
            break;
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
                case IDC_EDIT2:
                {
                    if (minfilter.handleeditbox(IDC_EDIT2)) rendertree(hDlg, false);
                    return (INT_PTR)TRUE;
                }
                /*
                case IDC_EDIT1:
                {
                    int sel = SendMessage(taglist, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
                    if(sel<0) break;
                    getcontroltext(GetDlgItem(hDlg, IDC_EDIT1), tags[sel].name, sizeof(tags[sel].name));
                    listitem(taglist, tags[sel].name, sel, sel, LVM_SETITEMA);
                    return (INT_PTR)TRUE;
                }
                */
                case IDC_COMBO1:
                {
                    if (HIWORD(wParam) == CBN_SELENDOK)
                    {
                        int sel = SendMessage(quickcombo, CB_GETCURSEL, 0, 0);
                        if (sel != CB_ERR)
                        {
                            endtime = now();

                            SYSTEMTIME st;
                            GetLocalTime(&st);

                            switch (sel)
                            {
                                case 0: starttime = endtime; break;  //"Today");
                                case 1:
                                    endtime--;
                                    starttime = endtime;
                                    break;  //"Yesterday");
                                case 2:
                                    starttime = endtime - ((st.wDayOfWeek == 1 ? 8 : st.wDayOfWeek) - 1);
                                    break;                                           //"Since Monday Morning");
                                case 3: starttime = endtime - 6; break;              //"Last 7 Days");
                                case 4: starttime = endtime - 13; break;             //"Last 14 Days");
                                case 5: starttime = endtime - (st.wDay - 1); break;  //"Month To Date");
                                case 6: starttime = endtime - 30; break;             //"30 Days");
                                case 7: starttime = endtime - 90; break;             //"90 Days");
                                case 8:
                                    st.wMonth = 1;
                                    st.wDay = 1;
                                    starttime = dayordering(st);
                                    break;  //"Year To Date");
                                case 9:
                                    st.wYear--;
                                    starttime = dayordering(st);
                                    break;                             //"Since A Year Ago");
                                case 10: starttime = firstday; break;  //"All Time");
                            }

                            setdaterangecontrols(hDlg);

                            rendertree(hDlg, true);
                            return (INT_PTR)TRUE;
                        }
                    }
                    break;
                }
                case IDC_EDIT8:
                {
                    String old(filterstrcontents);
                    getcontroltext(filterstr, filterstrcontents, 100);
                    String cur(filterstrcontents);
                    if (!(old == cur))
                    {
                        rendertree(hDlg, true);
                        return (INT_PTR)TRUE;
                    }
                    break;
                }
                case IDC_BUTTON2:
                {
                    int sel = SendMessage(taglist, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
                    if (sel >= 0 && selectednode && sel != selectednode->tag)
                    {
                        selectednode->tag = sel;
                        rendertree(hDlg, true);
                        return (INT_PTR)TRUE;
                    }
                    break;
                }
                case IDC_BUTTON3:
                {
                    int sel = SendMessage(taglist, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
                    if (sel < 0) break;

                    SetFocus(taglist);
                    ListView_EditLabel(taglist, sel);

                    break;
                }
                /*
                case IDC_COMBOBOXEX1:
                {
                    if(HIWORD(wParam)==LVN_SELCHANGE)
                    {
                        int sel = SendMessageA(tagdrop, CB_GETCURSEL, 0, 0);
                        if(sel!=CB_ERR && selectednode && sel!=selectednode->tag)
                        {
                            selectednode->tag = sel;
                            rendertree(hDlg, true);
                            return (INT_PTR)TRUE;
                        }
                    }
                    else if(HIWORD(wParam)==CBN_EDITCHANGE )
                    {
                        if(!selectednode) break;
                        HWND cb = (HWND)SendMessageA(tagdrop, CBEM_GETCOMBOCONTROL, 0, 0);
                        int sel = selectednode->tag;
                        GetWindowTextA(cb, tags[sel].name, sizeof(tags[sel].name));
                        tags[sel].name[sizeof(tags[sel].name)-1] = 0;
                        comboboxitem(tagdrop, tags[sel].name, sel, sel, CBEM_INSERTITEMA);
                        SendMessageA(cb, CBEM_DELETEITEM, sel+1, 0);
                        return (INT_PTR)TRUE;
                    }
                    else if(HIWORD(wParam)==CBN_KILLFOCUS)
                    {
                        //return (INT_PTR)TRUE;
                    }
                    break;
                }
                */
                case IDC_CHECK1:
                {
                    int sel = SendMessage(taglist, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
                    if (sel < 0) break;
                    // int sel = SendMessageA(tagdrop, CB_GETCURSEL, 0, 0);
                    if (IsDlgButtonChecked(hDlg, IDC_CHECK1) && sel != CB_ERR)
                        filterontag = sel;
                    else
                        filterontag = -1;
                    rendertree(hDlg, true);
                    return (INT_PTR)TRUE;
                }
            }
            break;
        }

        case WM_HSCROLL:
        {
            int fl = SendMessageA(foldslider, TBM_GETPOS, 0, 0);
            if (fl != foldlevel)
            {
                foldlevel = fl;
                rendertree(hDlg, false);
            }
            return (INT_PTR)TRUE;
        }

        case WM_NOTIFY:
        {
            long lResult = handleNotify(hDlg, (int)wParam, (LPNMHDR)lParam);
            SetWindowLong(hDlg, DWL_MSGRESULT, lResult);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
