#ifdef __MINGW32__
typedef TV_KEYDOWN NMTVKEYDOWN, *LPNMTVKEYDOWN;
// MS VS (MSDN) and g++ (MinGW) use different names for this type
// (the structure is defined in commctrl.h, included in stdafx.h);
// the NMTVKEYDOWN is used since Windows Vista / Windows Server 2003
// previous Windows versions used name TV_KEYDOWN, as MinGW does
#endif

const int barbmwidth = 1920;  // FIXME, screensize
const int barbmheight = 64;

void setbar(int tag, HDC hdc) {
    HBITMAP &bm = tags[tag].barbm;
    if (!bm) {
        if (!bitmapdc) bitmapdc = CreateCompatibleDC(hdc);
        bm = CreateCompatibleBitmap(hdc, barbmwidth, barbmheight);
        SelectObject(bitmapdc, bm);
        RECT r = {0, 0, barbmwidth, barbmheight};
        HBRUSH br = CreateSolidBrush(tags[tag].color);
        FillRect(bitmapdc, &r, br);
        DeleteObject(br);
    } else {
        SelectObject(bitmapdc, bm);
    }
}

const int controlmargin = 6;

// Ids of the Edit menu items, in the style the tray menu's are in.
enum {
    MENU_SELECTALL = 'SA',
    MENU_APPLYTAG = 'AT',
    MENU_OVERRIDE = 'MO',
    MENU_HIDE = 'HI',
    MENU_UNHIDE = 'UH',
    MENU_MERGESUB = 'MS',
    MENU_MERGE = 'MG',
    MENU_DELETE = 'DL',
};

HWND menutip = NULL;  // Menus have no tips of their own, this one gets tracked by hand.

// Tabs of the stats dialog, in the order they get inserted.
enum { TAB_STATISTICS = 0, TAB_TIME };

// How much time a single bar of the time graph covers, as offered by the period combo.
enum { PERIOD_DAY = 0, PERIOD_WEEK, PERIOD_MONTH, PERIOD_YEAR, PERIOD_NUM };

const char *periodnames[PERIOD_NUM] = {"Per Day", "Per Week", "Per Month", "Per Year"};
// The most days a period can cover, which bounds how much time one of its bars can hold.
const int perioddays[PERIOD_NUM] = {1, 7, 31, 366};

// Periods are drawn as columns wide enough to stay distinguishable, and scroll
// horizontally when more of them are in range than fit the window.
const int barminwidth = 20;
const int timegraphscrollstep = 40;  // Per scroll bar arrow click or wheel notch.
const char *timegraphclass = "PTTIMEGRAPH";
const char *barclickprompt = "Click a bar in the graph...";

// A day cannot hold more than 24 hours, so anything above what a period's days add up to
// is a collection error that would otherwise flatten every correct bar in the graph.
const DWORD daymaxseconds = 24 * 60 * 60;

HWND tabctrl = NULL;
HWND timegraph = NULL;
HWND periodcombo = NULL;
HWND normalizedcheck = NULL;
HWND periodlabel = NULL;
HWND periodtreeview = NULL;
HWND tagtimelist = NULL;
HBRUSH barselbrush = NULL;
int tabstripheight = 0;
int timegraphscroll = 0;
int graphperiod = PERIOD_DAY;
bool graphnormalized = false;

// Both tree views draw their bars from the same per node accumulation, which can only
// hold one date range at a time, so the graph keeps its own copy of the day totals
// while that accumulation gets narrowed to the period the period tree shows.
Vector<tagstat> graphdaystats;
int graphstartday = 0;
DWORD rangebargraphmax = 0, periodbargraphmax = 0;
bool accumisperiod = false;

// One bar per period that has any time on it: days without time get merged into the bar
// of their period, periods without any get no bar at all.
struct timebar {
    tagstat stat;
    int startday, endday;  // Day ordering of the first and last day with time in the period.
};
Vector<timebar> graphbars;
int selectedday = -1;  // A day inside the selected period, -1 when no bar is selected.
int selectedbar = -1;  // Index into graphbars of the bar holding it, -1 when there is none.

DWORD periodmaxseconds() { return daymaxseconds * perioddays[graphperiod]; }

// Days that share this land in the same bar. Month and year come straight out of the day
// ordering, which packs both, but weeks have to be counted in actual days.
int periodkey(int nday) {
    switch (graphperiod) {
        case PERIOD_WEEK: return weekordering(nday);
        case PERIOD_MONTH: return nday / monthfactor;
        case PERIOD_YEAR: return nday / yearfactor;
        default: return nday;
    }
}

// Keeps the selection on the same period across a rebuild, whether the bars changed
// because of the date range or because of the period, and drops it when the day it was
// on no longer has a bar.
void findselectedbar() {
    selectedbar = -1;
    if (selectedday < 0) return;
    loopv(i, graphbars) {
        if (selectedday >= graphbars[i].startday && selectedday <= graphbars[i].endday) {
            selectedbar = i;
            return;
        }
    }
    selectedday = -1;
}

void buildgraphbars() {
    graphbars.setsize(0);
    loopv(i, graphdaystats) {
        tagstat &ds = graphdaystats[i];
        if (!ds.total) continue;
        int nday = graphstartday + i;
        if (!graphbars.empty() && periodkey(graphbars.last().startday) == periodkey(nday)) {
            timebar &tb = graphbars.last();
            tb.endday = nday;
            tb.stat.add(ds);
            tb.stat.total += ds.total;
        } else {
            timebar tb;
            tb.stat = ds;
            tb.startday = tb.endday = nday;
            graphbars.push(tb);
        }
    }
    findselectedbar();
}

struct timegraphinfo {
    int numbars;
    DWORD biggesttotal;
    int barwidth;
    int totalwidth;
};

// Bar sizing has to agree between painting, hit testing and the scroll bar range.
timegraphinfo computetimegraph(int availwidth) {
    timegraphinfo tg = {graphbars.size(), 0, 0, 0};
    DWORD maxsecs = periodmaxseconds();
    loopv(i, graphbars) {
        DWORD sec = graphbars[i].stat.total;
        if (sec > tg.biggesttotal) tg.biggesttotal = sec;
    }
    if (tg.biggesttotal > maxsecs) tg.biggesttotal = maxsecs;
    if (!tg.numbars) return tg;
    tg.barwidth = availwidth / tg.numbars;
    if (tg.barwidth < barminwidth) tg.barwidth = barminwidth;
    tg.totalwidth = tg.numbars * tg.barwidth;
    return tg;
}

// The bars get drawn inside a one pixel frame.
RECT timegrapharea(HWND hwnd) {
    RECT r;
    GetClientRect(hwnd, &r);
    InflateRect(&r, -1, -1);
    return r;
}

void updatetimegraphscroll() {
    if (!timegraph) return;
    RECT r = timegrapharea(timegraph);
    int avail = r.right - r.left;
    timegraphinfo tg = computetimegraph(avail);
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = tg.totalwidth ? tg.totalwidth - 1 : 0;
    si.nPage = avail > 0 ? avail : 1;
    si.nPos = timegraphscroll;
    SetScrollInfo(timegraph, SB_HORZ, &si, TRUE);
    // The scroll bar clamps the position to the range it was just given.
    timegraphscroll = GetScrollPos(timegraph, SB_HORZ);
}

void scrolltimegraph(HWND hwnd, int pos) {
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE;
    GetScrollInfo(hwnd, SB_HORZ, &si);
    int maxpos = si.nMax - (int)si.nPage + 1;
    if (pos > maxpos) pos = maxpos;
    if (pos < si.nMin) pos = si.nMin;
    if (pos == timegraphscroll) return;
    timegraphscroll = pos;
    SetScrollPos(hwnd, SB_HORZ, pos, TRUE);
    InvalidateRect(hwnd, NULL, TRUE);
}

void rendertimegraph(HDC hdc, HWND hwnd) {
    RECT r;
    GetClientRect(hwnd, &r);
    FillRect(hdc, &r, greybrush);
    r = timegrapharea(hwnd);
    FillRect(hdc, &r, whitebrush);
    int availh = r.bottom - r.top;
    timegraphinfo tg = computetimegraph(r.right - r.left);
    if (!tg.biggesttotal) return;
    // Wide bars get a wider gap between them, narrow ones would disappear into it.
    int gap = tg.barwidth / 8;
    if (gap < 1) gap = 1;
    if (gap > 6) gap = 6;
    DWORD maxsecs = periodmaxseconds();
    loopv(i, graphbars) {
        int x = r.left + i * tg.barwidth - timegraphscroll;
        RECT rt = {x, r.top, x + tg.barwidth - gap, r.bottom};
        if (rt.right > r.left && rt.left < r.right) {
            if (rt.left < r.left) rt.left = r.left;
            if (rt.right > r.right) rt.right = r.right;
            RECT bar = rt;
            if (i == selectedbar) FillRect(hdc, &rt, barselbrush);
            // A normalized bar is scaled to its own total, so the tags in it read as the
            // proportion of the time that period holds instead of of the busiest one.
            DWORD scale = graphnormalized ? graphbars[i].stat.total : tg.biggesttotal;
            if (scale > maxsecs) scale = maxsecs;
            // Bars only exist for periods with time on them, so the scale is never zero.
            DWORD bartotal = graphbars[i].stat.total;
            if (bartotal > scale) bartotal = scale;
            int barheight = (int)((unsigned long long)bartotal * availh / scale);
            // Bars hang centred rather than standing on the floor, which puts the middle
            // of every one of them on the same line and so lines the tags inside bars of
            // different heights up far better than a shared bottom edge does. A normalized
            // bar already fills the height, which leaves it where it was.
            int base = r.bottom - (availh - barheight) / 2;
            rt.bottom = base;
            DWORD acc = 0;
            loop(j, MAXTAGS) {
                acc += graphbars[i].stat.seconds[j];
                // Measuring each segment's top off the running total instead of stacking
                // heights keeps rounding from leaving a gap at the top of a bar that
                // fills the whole scale.
                DWORD upto = acc < scale ? acc : scale;
                // A year of seconds times the pixels to scale it by doesn't fit 32 bits.
                int top = base - (int)((unsigned long long)upto * availh / scale);
                // A period over its own scale gets its bar truncated at the top.
                if (top < r.top) top = r.top;
                if (top < rt.bottom) {
                    rt.top = top;
                    if (!tags[j].br) tags[j].br = CreateSolidBrush(tags[j].color);
                    FillRect(hdc, &rt, tags[j].br);
                    rt.bottom = rt.top;
                    if (rt.bottom <= r.top) break;
                }
            }
            // Normalized bars all reach the top, which leaves the selected one's
            // backing fill covered up, so it gets an outline of its own as well.
            if (i == selectedbar) FrameRect(hdc, &bar, (HBRUSH)GetStockObject(BLACK_BRUSH));
        }
    }
}

// Defined below, once the tree filling it shares with the statistics tab exists.
void selectbar(int bar);

// Index of the bar drawn under the given point, or -1 if there is none.
int timegraphhit(HWND hwnd, int x, int y) {
    RECT r = timegrapharea(hwnd);
    timegraphinfo tg = computetimegraph(r.right - r.left);
    if (!tg.barwidth || x < r.left || x >= r.right || y < r.top || y >= r.bottom) return -1;
    int index = (x - r.left + timegraphscroll) / tg.barwidth;
    return index < tg.numbars ? index : -1;
}

void setperiodlabel() {
    if (selectedbar < 0) {
        SetWindowTextA(periodlabel, barclickprompt);
        return;
    }
    int nday = graphbars[selectedbar].startday;
    daydata d;
    // The first day of a week that has time on it isn't necessarily the Monday it started on.
    d.nday = (WORD)(graphperiod == PERIOD_WEEK ? weekstart(nday) : nday);
    SYSTEMTIME st;
    d.createsystime(st);
    String s;
    switch (graphperiod) {
        case PERIOD_WEEK: s.Format("Week of %d-%d-%d", st.wYear, st.wMonth, st.wDay); break;
        case PERIOD_MONTH: s.Format("%d-%d", st.wYear, st.wMonth); break;
        case PERIOD_YEAR: s.Format("%d", st.wYear); break;
        default: s.Format("%d-%d-%d", st.wYear, st.wMonth, st.wDay); break;
    }
    SetWindowTextA(periodlabel, s.c_str());
}

// The bars name their tags only by colour, so the selected one's tags get listed with
// what each of them holds, most time first to match how they stack up in the bar.
void filltagtimelist() {
    if (!tagtimelist) return;
    SendMessageA(tagtimelist, LVM_DELETEALLITEMS, 0, 0);
    if (selectedbar < 0) return;
    tagstat &ts = graphbars[selectedbar].stat;
    int order[MAXTAGS], num = 0;
    loop(i, MAXTAGS) if (ts.seconds[i]) order[num++] = i;
    loop(i, num) loop(j, num - 1 - i) {
        if (ts.seconds[order[j]] < ts.seconds[order[j + 1]]) {
            int swap = order[j];
            order[j] = order[j + 1];
            order[j + 1] = swap;
        }
    }
    loop(i, num) {
        listitem(tagtimelist, tags[order[i]].name, i, order[i], LVM_INSERTITEMA);
        // The time of day formatting is what the tree uses, at the level that always
        // spells out hours and minutes so the column reads as one.
        daydata d;
        d.seconds = ts.seconds[order[i]];
        String s;
        d.format(s, 2);
        LVITEMA lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 1;
        lvi.pszText = (char *)s.c_str();
        SendMessageA(tagtimelist, LVM_SETITEMA, 0, (LPARAM)&lvi);
    }
}

// Everything in the panel beside the graph is about whichever bar is selected.
void showselectedperiod() {
    setperiodlabel();
    filltagtimelist();
}

LRESULT CALLBACK TimeGraphProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            rendertimegraph(hdc, hwnd);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_SIZE:
            updatetimegraphscroll();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        case WM_LBUTTONDOWN: {
            // So the wheel and the arrow keys scroll the graph the user just clicked on.
            SetFocus(hwnd);
            int bar = timegraphhit(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (bar >= 0) selectbar(bar);
            return 0;
        }
        case WM_GETDLGCODE: return DLGC_WANTARROWS;
        case WM_MOUSEWHEEL:
            scrolltimegraph(hwnd, timegraphscroll - GET_WHEEL_DELTA_WPARAM(wParam) *
                                                        timegraphscrollstep / WHEEL_DELTA);
            return 0;
        case WM_KEYDOWN: {
            SCROLLINFO si;
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_RANGE | SIF_PAGE;
            GetScrollInfo(hwnd, SB_HORZ, &si);
            switch (wParam) {
                case VK_LEFT:
                    scrolltimegraph(hwnd, timegraphscroll - timegraphscrollstep);
                    return 0;
                case VK_RIGHT:
                    scrolltimegraph(hwnd, timegraphscroll + timegraphscrollstep);
                    return 0;
                case VK_PRIOR: scrolltimegraph(hwnd, timegraphscroll - si.nPage); return 0;
                case VK_NEXT: scrolltimegraph(hwnd, timegraphscroll + si.nPage); return 0;
                case VK_HOME: scrolltimegraph(hwnd, si.nMin); return 0;
                case VK_END: scrolltimegraph(hwnd, si.nMax); return 0;
            }
            break;
        }
        case WM_HSCROLL: {
            SCROLLINFO si;
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_HORZ, &si);
            int pos = si.nPos;
            switch (LOWORD(wParam)) {
                case SB_LINELEFT: pos -= timegraphscrollstep; break;
                case SB_LINERIGHT: pos += timegraphscrollstep; break;
                case SB_PAGELEFT: pos -= si.nPage; break;
                case SB_PAGERIGHT: pos += si.nPage; break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: pos = si.nTrackPos; break;
                case SB_LEFT: pos = si.nMin; break;
                case SB_RIGHT: pos = si.nMax; break;
                default: return 0;
            }
            scrolltimegraph(hwnd, pos);
            return 0;
        }
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

void registertimegraphclass() {
    WNDCLASSEXA wcex;
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.lpfnWndProc = TimeGraphProc;
    wcex.hInstance = hInst;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = timegraphclass;
    if (!RegisterClassExA(&wcex)) panic("PT: Cannot register time graph window class");
}

void redrawtreeview() {
    RECT r;
    GetClientRect(treeview, &r);
    InvalidateRect(treeview, &r, TRUE);
}

// The tree view control has no multi selection of its own: node::selected holds it, and the
// items get their TVIS_SELECTED state set to match. selectednode stays the node the caret is
// on, which is what the operations that can only work on one node still go by.
node *selanchor = NULL;       // What a shift click or shift arrow extends the range from.
WNDPROC treeviewproc = NULL;  // The tree view's own handler, subclassed for multi select.
bool settingtreesel = false;  // Set while a selection change originates from the code below.

node *treeitemnode(HWND tv, HTREEITEM h) {
    TVITEM tvi = {0};
    tvi.mask = TVIF_HANDLE | TVIF_PARAM;
    tvi.hItem = h;
    return TreeView_GetItem(tv, &tvi) ? (node *)tvi.lParam : NULL;
}

// Walks all items, including the ones a collapsed parent keeps off screen.
HTREEITEM nexttreeitem(HWND tv, HTREEITEM h) {
    HTREEITEM n = TreeView_GetChild(tv, h);
    if (n) return n;
    for (; h; h = TreeView_GetParent(tv, h)) {
        n = TreeView_GetNextSibling(tv, h);
        if (n) return n;
    }
    return NULL;
}

void settreeitemselected(HWND tv, HTREEITEM h, bool sel) {
    TVITEM tvi = {0};
    tvi.mask = TVIF_HANDLE | TVIF_STATE;
    tvi.hItem = h;
    tvi.stateMask = TVIS_SELECTED;
    tvi.state = sel ? TVIS_SELECTED : 0;
    TreeView_SetItem(tv, &tvi);
}

// Operations work on the selected nodes the tree holds, in the order it lists them.
#define loopselected(n)                                           \
    for (HTREEITEM selitem = TreeView_GetRoot(treeview); selitem; \
         selitem = nexttreeitem(treeview, selitem))               \
        if (node *n = treeitemnode(treeview, selitem))            \
            if (n->selected)

node *firstselectednode() {
    loopselected(n) return n;
    return NULL;
}

// Puts the highlight on exactly the items whose node is selected, which is all there is to
// showing the selection.
void refreshtreeselection() {
    for (HTREEITEM h = TreeView_GetRoot(treeview); h; h = nexttreeitem(treeview, h)) {
        TVITEM tvi = {0};
        tvi.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
        tvi.hItem = h;
        tvi.stateMask = TVIS_SELECTED;
        if (!TreeView_GetItem(treeview, &tvi)) continue;
        node *n = (node *)tvi.lParam;
        bool sel = n && n->selected;
        // This runs over the whole tree, so items that are already right are left alone
        // rather than being marked for a redraw they don't need.
        if (sel != ((tvi.state & TVIS_SELECTED) != 0)) settreeitemselected(treeview, h, sel);
    }
}

void selectonly(node *n) {
    root->clearselected();
    if (n) n->selected = true;
    selanchor = n;
}

// The item a node is shown in, or NULL when it is folded away or gone from the tree.
HTREEITEM visibletreeitem(node *n) {
    for (HTREEITEM h = TreeView_GetRoot(treeview); h; h = TreeView_GetNextVisible(treeview, h))
        if (treeitemnode(treeview, h) == n) return h;
    return NULL;
}

// Everything between two items in display order, which is what a shift select spans.
void selectrange(HTREEITEM from, HTREEITEM to) {
    root->clearselected();
    int edges = 0;
    for (HTREEITEM h = TreeView_GetRoot(treeview); h; h = TreeView_GetNextVisible(treeview, h)) {
        if (h == from) edges++;
        if (h == to) edges++;
        if (edges) {
            node *n = treeitemnode(treeview, h);
            if (n) n->selected = true;
        }
        // Both ends have been passed, whichever of the two came first.
        if (edges >= 2) break;
    }
}

void extendselection(node *anchor, HTREEITEM to) {
    HTREEITEM from = anchor ? visibletreeitem(anchor) : NULL;
    selectrange(from ? from : to, to);
    // Shift selecting again keeps growing the range from the same anchor.
    selanchor = anchor ? anchor : treeitemnode(treeview, to);
}

// Ctrl-A takes everything the tree currently shows, so nodes folded away inside a collapsed
// parent stay out of it.
void selectalltreeitems() {
    root->clearselected();
    for (HTREEITEM h = TreeView_GetRoot(treeview); h; h = TreeView_GetNextVisible(treeview, h)) {
        node *n = treeitemnode(treeview, h);
        if (n) n->selected = true;
    }
    selanchor = firstselectednode();
    if (!selectednode) selectednode = selanchor;
    refreshtreeselection();
}

// Moving the caret makes the control select that item by itself, so the rest of the
// selection has to be put back after it.
void settreecaret(HTREEITEM h) {
    settingtreesel = true;
    TreeView_SelectItem(treeview, h);
    settingtreesel = false;
    refreshtreeselection();
}

// A ctrl click toggles a single node, a shift click replaces the selection with the range
// from wherever the last plain or ctrl click left the anchor.
void treeselectclick(HTREEITEM h, bool shift) {
    node *n = treeitemnode(treeview, h);
    if (!n) return;
    if (shift) {
        extendselection(selanchor, h);
    } else {
        n->selected = !n->selected;
        selanchor = n;
    }
    prevselectednode = selectednode;
    selectednode = n->selected ? n : firstselectednode();
    settreecaret(h);
}

// The control does no multi selection, so ctrl and shift get dealt with before it sees them.
LRESULT CALLBACK TreeViewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_LBUTTONDOWN: {
            TVHITTESTINFO ht;
            ht.pt.x = GET_X_LPARAM(lParam);
            ht.pt.y = GET_Y_LPARAM(lParam);
            HTREEITEM h = TreeView_HitTest(hwnd, &ht);
            if (h && (ht.flags & TVHT_ONITEM)) {
                if (GetKeyState(VK_CONTROL) & 0x8000 || GetKeyState(VK_SHIFT) & 0x8000) {
                    SetFocus(hwnd);
                    treeselectclick(h, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
                    return 0;
                }
                // A plain click on the item the caret is already on is no change to the
                // control, so it reports none and the rest of the selection gets dropped here.
                if (h == TreeView_GetSelection(hwnd)) {
                    LRESULT r = CallWindowProc(treeviewproc, hwnd, message, wParam, lParam);
                    selectonly(treeitemnode(hwnd, h));
                    refreshtreeselection();
                    return r;
                }
            }
            break;
        }
        case WM_KEYDOWN: {
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                if (wParam == 'A') {
                    selectalltreeitems();
                    return 0;
                }
            } else if (GetKeyState(VK_SHIFT) & 0x8000) {
                switch (wParam) {
                    case VK_UP:
                    case VK_DOWN:
                    case VK_PRIOR:
                    case VK_NEXT:
                    case VK_HOME:
                    case VK_END: {
                        // The control moves the caret first, which cuts the selection back to
                        // just that one item.
                        node *anchor = selanchor;
                        LRESULT r = CallWindowProc(treeviewproc, hwnd, message, wParam, lParam);
                        HTREEITEM caret = TreeView_GetSelection(hwnd);
                        if (caret) {
                            extendselection(anchor, caret);
                            refreshtreeselection();
                        }
                        return r;
                    }
                }
            }
            break;
        }
    }
    return CallWindowProc(treeviewproc, hwnd, message, wParam, lParam);
}

void recompaccum() {
    root->checkstrfilter(false);
    daydata d;
    tagstat ts;
    daystats.setsize(0);
    loop(i, endtime - starttime + 1) daystats.push(tagstat());
    root->accumulate(d, ts);
}

// Accumulates the whole date range: what the statistics tree draws from, and where
// the time graph takes its copy of the per day totals.
void accumulaterange() {
    recompaccum();
    loopv(i, daystats) daystats[i].sum();
    graphdaystats.setsize(0);
    loopv(i, daystats) graphdaystats.push(daystats[i]);
    graphstartday = starttime;
    buildgraphbars();
    maxsecondsforbargraph = rangebargraphmax;
    accumisperiod = false;
}

// Narrows the accumulation to the period the period tree shows, leaving the graph on
// its copy. The days without time inside the period contribute nothing, so the bar's
// outermost days with time are range enough.
void accumulateperiod() {
    int savedstart = starttime, savedend = endtime;
    starttime = graphbars[selectedbar].startday;
    endtime = graphbars[selectedbar].endday;
    recompaccum();
    starttime = savedstart;
    endtime = savedend;
    maxsecondsforbargraph = periodbargraphmax;
    accumisperiod = true;
}

// Puts whatever is currently accumulated in a tree view.
void filltree(HWND tv) {
    // Emptying the tree can report a selection change that isn't one the user made.
    settingtreesel = true;
    TreeView_DeleteAllItems(tv);
    root->treeview(0, tv, NULL, TVI_ROOT);
    settingtreesel = false;
}

void renderperiodtree() {
    if (!periodtreeview) return;
    if (selectedbar < 0) {
        TreeView_DeleteAllItems(periodtreeview);
        if (accumisperiod) accumulaterange();
        return;
    }
    accumulateperiod();
    filltree(periodtreeview);
    periodbargraphmax = maxsecondsforbargraph;
}

void rendertree(HWND hDlg) {
    accumulaterange();
    filltree(treeview);
    // The items are all new ones, so the selection has to be put back onto them.
    refreshtreeselection();
    rangebargraphmax = maxsecondsforbargraph;
    if (timegraph) {
        // The bars were just rebuilt, which is where a selection that fell out of the
        // range or lost all its time gets dropped.
        showselectedperiod();
        // The period tree is hidden here, it gets rebuilt when its tab is shown again.
        updatetimegraphscroll();
        InvalidateRect(timegraph, NULL, TRUE);
    }
}

void selectbar(int bar) {
    if (bar == selectedbar) return;
    selectedbar = bar;
    selectedday = bar < 0 ? -1 : graphbars[bar].startday;
    showselectedperiod();
    renderperiodtree();
    InvalidateRect(timegraph, NULL, TRUE);
}

void setgraphnormalized(bool normalized) {
    if (normalized == graphnormalized) return;
    graphnormalized = normalized;
    InvalidateRect(timegraph, NULL, TRUE);
}

// Regrouping the same days into longer or shorter bars keeps whichever period the
// selected day ended up in, so the tree next to the graph follows along.
void setgraphperiod(int period) {
    if (period == graphperiod) return;
    graphperiod = period;
    buildgraphbars();
    showselectedperiod();
    renderperiodtree();
    updatetimegraphscroll();
    // Bars of a different width put the interesting end of the range elsewhere.
    SendMessageA(timegraph, WM_HSCROLL, SB_RIGHT, 0);
    InvalidateRect(timegraph, NULL, TRUE);
}

// The tab strip spans the top, the page below it holds either the tree or the time graph.
void layoutstats(HWND hDlg) {
    RECT r;
    GetClientRect(hDlg, &r);
    MoveWindow(tabctrl, 0, 0, r.right, tabstripheight, TRUE);
    int pagetop = tabstripheight + controlmargin;
    RECT rsl;
    GetWindowRect(foldslider, &rsl);
    int treeleft = rsl.right - rsl.left + 12;
    MoveWindow(treeview, treeleft, pagetop, r.right - treeleft, r.bottom - pagetop - controlmargin,
               TRUE);
    // The graph's own controls and the bar of it that was clicked on get a quarter of
    // the width to themselves.
    int panelright = r.right / 4;
    int panelwidth = panelright - controlmargin;
    RECT rc;
    GetWindowRect(periodcombo, &rc);
    int comboheight = rc.bottom - rc.top;
    int itemheight = (int)SendMessageA(periodcombo, CB_GETITEMHEIGHT, 0, 0);
    // The height a combo gets sized to covers its drop down list, the control itself
    // stays at the closed height that was just read back off it.
    MoveWindow(periodcombo, controlmargin, pagetop, panelwidth,
               comboheight + PERIOD_NUM * itemheight, TRUE);
    RECT rn;
    GetWindowRect(normalizedcheck, &rn);
    int checkheight = rn.bottom - rn.top;
    int checktop = pagetop + comboheight + controlmargin;
    MoveWindow(normalizedcheck, controlmargin, checktop, panelwidth, checkheight, TRUE);
    RECT rl;
    GetWindowRect(periodlabel, &rl);
    int labelheight = rl.bottom - rl.top;
    int labeltop = checktop + checkheight + controlmargin;
    MoveWindow(periodlabel, controlmargin, labeltop, panelwidth, labelheight, TRUE);
    int periodtreetop = labeltop + labelheight + controlmargin;
    // The tag list under the tree gets a third of the height the two of them share.
    int panelrest = r.bottom - controlmargin - periodtreetop - controlmargin;
    if (panelrest < 0) panelrest = 0;
    int listheight = panelrest / 3;
    int treeheight = panelrest - listheight;
    MoveWindow(periodtreeview, controlmargin, periodtreetop, panelwidth, treeheight, TRUE);
    int listtop = periodtreetop + treeheight + controlmargin;
    MoveWindow(tagtimelist, controlmargin, listtop, panelwidth, listheight, TRUE);
    // The time column keeps its width, the tag names take whatever is left over.
    int timecol = 70;
    int namecol = panelwidth - timecol - GetSystemMetrics(SM_CXVSCROLL);
    if (namecol < 40) namecol = 40;
    SendMessageA(tagtimelist, LVM_SETCOLUMNWIDTH, 0, namecol);
    SendMessageA(tagtimelist, LVM_SETCOLUMNWIDTH, 1, timecol);
    int graphleft = panelright + controlmargin;
    MoveWindow(timegraph, graphleft, pagetop, r.right - graphleft - controlmargin,
               r.bottom - pagetop - controlmargin, TRUE);
    InvalidateRect(hDlg, NULL, TRUE);
}

void showtab(HWND hDlg, int tab) {
    // Only one tree is on screen at a time, so the accumulation they share follows the tab.
    if (tab == TAB_TIME)
        renderperiodtree();
    else if (accumisperiod)
        accumulaterange();
    // Only the immediate children belong to a page, anything below them (such as
    // the tag list's label editor) is the business of the control owning it.
    for (HWND h = GetWindow(hDlg, GW_CHILD); h; h = GetWindow(h, GW_HWNDNEXT)) {
        if (h == tabctrl) continue;
        bool ontime = h == timegraph || h == periodcombo || h == normalizedcheck ||
                      h == periodlabel || h == periodtreeview || h == tagtimelist;
        ShowWindow(h, ontime == (tab == TAB_TIME) ? SW_SHOW : SW_HIDE);
    }
}

long handleCustomDraw(HWND hWndTreeView, LPNMTVCUSTOMDRAW pNMTVCD) {
    if (pNMTVCD == NULL) return -1;
    switch (pNMTVCD->nmcd.dwDrawStage) {
        case CDDS_PREPAINT: return CDRF_NOTIFYPOSTPAINT | CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: return CDRF_NOTIFYPOSTPAINT;
        case CDDS_ITEMPOSTPAINT: {
            RECT tvr;
            GetClientRect(hWndTreeView, &tvr);
            int bargraphwidth = tvr.right;
            RECT rc;
            TreeView_GetItemRect(hWndTreeView, (HTREEITEM)pNMTVCD->nmcd.dwItemSpec, &rc, 1);
            HTREEITEM hItem = (HTREEITEM)pNMTVCD->nmcd.dwItemSpec;
            TVITEM tvi = {0};
            tvi.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
            tvi.hItem = hItem;
            tvi.stateMask = -1;
            TreeView_GetItem(hWndTreeView, &tvi);
            if (tvi.lParam) {
                if (tvi.state & TVIS_EXPANDED && TreeView_GetChild(hWndTreeView, hItem)) {
                    rc.left = rc.right;
                    rc.right = bargraphwidth;
                    FillRect(pNMTVCD->nmcd.hdc, &rc, whitebrush);
                } else {
                    node *n = (node *)tvi.lParam;
                    tagstat ts;
                    if (n->ts)
                        ts = *n->ts;
                    else
                        ts.seconds[n->gettag()] = n->accum.seconds;
                    int totalpixels =
                        n->accum.seconds / (float)maxsecondsforbargraph * (bargraphwidth - rc.left);
                    loop(i, MAXTAGS) if (ts.seconds[i]) {
                        setbar(i, pNMTVCD->nmcd.hdc);
                        int segwidth = totalpixels / (float)n->accum.seconds * ts.seconds[i];
                        rc.right = rc.left + segwidth;
                        BitBlt(pNMTVCD->nmcd.hdc, rc.left, rc.top, rc.right - rc.left,
                               rc.bottom - rc.top, bitmapdc, 0, 0, SRCAND);
                        rc.left += segwidth;
                    }
                }
            }
            return CDRF_DODEFAULT;
        } break;
    }
    return 0;
}

bool ApplyTagToNode(HWND hDlg) {
    int sel = SendMessage(taglist, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
    if (sel < 0) return false;
    bool changed = false;
    loopselected(n) if (n->tag != sel) {
        n->tag = sel;
        changed = true;
    }
    if (changed) rendertree(hDlg);
    return changed;
}

void scaleselection(HWND hDlg) {
    // Nodes without any time of their own have nothing to scale.
    bool anytime = false;
    loopselected(n) if (n->last) anytime = true;
    if (!anytime) return;
    char buf[100] = "100";
    if (CWin32InputBox::InputBox("Manual Override",
                                 "Enter percentage to scale the selected nodes by (100 = no "
                                 "change)",
                                 buf, 100, false, hDlg) != IDOK)
        return;
    loopselected(n) n->changetime(atoi(buf));
    rendertree(hDlg);
}

void hideselection(HWND hDlg) {
    bool changed = false;
    loopselected(n) if (n != root) {
        n->hidden = true;
        changed = true;
    }
    if (changed) rendertree(hDlg);
}

void unhideselection(HWND hDlg) {
    bool changed = false;
    loopselected(n) {
        n->clearhidden();
        changed = true;
    }
    if (changed) rendertree(hDlg);
}

// Merging in the siblings deletes them, and they may well be selected themselves, so this
// one only ever runs on a single node.
void mergesubstringsiblings(HWND hDlg) {
    int count = 0;
    loopselected(n) count++;
    if (count != 1) {
        MessageBoxA(hDlg,
                    "This merges the siblings whose name contains the selected node's name "
                    "into it, so it needs exactly one node selected.",
                    "Merge Substring Siblings", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    node *first = firstselectednode();
    first->firstinchain()->mergallsubstring();
    // Whatever was merged in has been deleted, selection and all.
    selectonly(first);
    selectednode = first;
    prevselectednode = NULL;
    rendertree(hDlg);
}

// The selection with each chain collapsed to its head and duplicates dropped. The caller
// has to empty this with setsize_nd, since a Vector deletes the pointers left in it.
void getselectedchains(Vector<node *> &v) {
    loopselected(n) {
        node *f = n->firstinchain();
        // The root has no parent to be taken out of, so it never takes part in a merge.
        if (!f->parent) continue;
        bool dup = false;
        loopv(i, v) if (v[i] == f) dup = true;
        if (!dup) v.push(f);
    }
}

// Merging a node into one below it would throw the merged data away along with it.
bool mergeableinto(node *o, node *target) { return o != target && !o->isancestorof(target); }

// Nodes that are the same app or site under a changed title format get merged into one.
// Which one that is follows from the data: the one still being added to is the one whose
// name the thing currently goes under.
void mergeselection(HWND hDlg) {
    Vector<node *> sel;
    getselectedchains(sel);
    if (sel.size() < 2) {
        sel.setsize_nd(0);
        MessageBoxA(hDlg,
                    "This merges all selected nodes into the one of them that was added to "
                    "most recently, so it needs two or more nodes selected.",
                    "Merge Nodes", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    node *target = NULL;
    DWORD recent = 0;
    loopv(i, sel) {
        DWORD t = sel[i]->lastactivity();
        if (!target || t > recent) {
            recent = t;
            target = sel[i];
        }
    }
    // Everything gets picked before anything is deleted, as the nodes that go away take
    // their whole subtree with them.
    Vector<node *> tomerge;
    loopv(i, sel) {
        node *o = sel[i];
        if (!mergeableinto(o, target)) continue;
        // A node inside another one that is about to be merged comes along with it.
        bool covered = false;
        loopv(j, sel) if (sel[j] != o && mergeableinto(sel[j], target) &&
                          sel[j]->isancestorof(o)) covered = true;
        if (!covered) tomerge.push(o);
    }
    loopv(i, tomerge) {
        target->merge(*tomerge[i]);
        tomerge[i]->parent->remove(tomerge[i]);
    }
    bool merged = !tomerge.empty();
    tomerge.setsize_nd(0);
    sel.setsize_nd(0);
    if (merged) {
        // The nodes that went away took their selection with them.
        selectonly(target);
        selectednode = target;
        prevselectednode = NULL;
        rendertree(hDlg);
    }
}

// The nodes an item on screen stands for, with duplicates dropped and anything already
// covered by a selected ancestor left out, as deleting that frees it too. The caller has to
// empty this with setsize_nd, since a Vector deletes the pointers left in it.
void getselectedviewchains(Vector<node *> &v) {
    Vector<node *> heads;
    loopselected(n) {
        node *f = n->firstinviewchain();
        // The root has no parent to be taken out of, so it can never be deleted.
        if (!f->parent) continue;
        bool dup = false;
        loopv(i, heads) if (heads[i] == f) dup = true;
        if (!dup) heads.push(f);
    }
    loopv(i, heads) {
        bool covered = false;
        loopv(j, heads) if (heads[j] != heads[i] && heads[j]->isancestorof(heads[i]))
            covered = true;
        if (!covered) v.push(heads[i]);
    }
    heads.setsize_nd(0);
}

// Takes the selected nodes out of the database along with everything below them. The
// tracker adds a node back the next time that app or site comes up, so what this loses for
// good is the time that was on them.
void deleteselection(HWND hDlg) {
    Vector<node *> todelete;
    getselectedviewchains(todelete);
    if (todelete.empty()) {
        todelete.setsize_nd(0);
        MessageBoxA(hDlg, "This deletes the selected nodes, so it needs a selection to work on.",
                    "Delete Nodes", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    String msg;
    msg.Format("Delete %d selected node%s, everything below %s, and all the time recorded on "
               "any of it?\r\n\r\nThis covers every date rather than just the range in view, "
               "and cannot be undone.",
               todelete.size(), todelete.size() == 1 ? "" : "s",
               todelete.size() == 1 ? "it" : "them");
    // The message box pumps messages, so the tracker can run while it is up. It only ever
    // adds nodes, which leaves everything picked above where it was.
    if (MessageBoxA(hDlg, msg.c_str(), "Delete Nodes", MB_OKCANCEL | MB_ICONWARNING) != IDOK) {
        todelete.setsize_nd(0);
        return;
    }
    loopv(i, todelete) todelete[i]->parent->remove(todelete[i]);
    todelete.setsize_nd(0);
    // The nodes that went away took the selection with them.
    selectonly(NULL);
    selectednode = prevselectednode = NULL;
    rendertree(hDlg);
    // The period tree has items of its own on these nodes, and it draws from them, so while
    // it is the tree on screen it gets rebuilt here rather than when its tab comes back.
    if (TabCtrl_GetCurSel(tabctrl) == TAB_TIME) renderperiodtree();
}

const char *menuitemtip(UINT id) {
    switch (id) {
        case MENU_SELECTALL:
            return "Selects every node the tree is currently showing. Nodes folded away inside "
                   "a collapsed parent are not included.";
        case MENU_APPLYTAG:
            return "Gives every selected node the tag that is highlighted in the tag list "
                   "below.\r\nWarning: irreversible, the tags it replaces are not remembered.";
        case MENU_OVERRIDE:
            return "Scales the time recorded on every selected node by a percentage, for "
                   "correcting time that was tracked wrongly.\r\nWarning: irreversible.";
        case MENU_HIDE:
            return "Takes every selected node out of the tree. They can be brought back with "
                   "Unhide on a node above them.";
        case MENU_UNHIDE:
            return "Brings back everything hidden below the selected nodes.";
        case MENU_MERGESUB:
            return "Merges the siblings whose name contains the selected node's name into it, "
                   "for names that only differ by what got appended to them. Needs exactly one "
                   "node selected.\r\nWarning: irreversible, the siblings are deleted.";
        case MENU_MERGE:
            return "Merges all selected nodes into whichever of them was added to most "
                   "recently, for one app or site that has been recorded under several names."
                   "\r\nWarning: irreversible, the other nodes are deleted.";
        case MENU_DELETE:
            return "Removes every selected node and everything below it from the database, "
                   "with all the time recorded on any of it.\r\nWarning: irreversible, and "
                   "it covers every date rather than just the range in view.";
    }
    return NULL;
}

// A tip only shows while the menu it belongs to is open, so it gets placed by hand rather
// than by the tool it is attached to.
void showmenutip(const char *text) {
    if (!menutip) return;
    TOOLINFOA ti = {0};
    ti.cbSize = sizeof(ti);
    ti.hwnd = statsdialog;
    ti.uId = (UINT_PTR)statsdialog;
    if (!text) {
        SendMessageA(menutip, TTM_TRACKACTIVATE, FALSE, (LPARAM)&ti);
        return;
    }
    ti.lpszText = (LPSTR)text;
    SendMessageA(menutip, TTM_UPDATETIPTEXTA, 0, (LPARAM)&ti);
    POINT pt;
    GetCursorPos(&pt);
    SendMessageA(menutip, TTM_TRACKPOSITION, 0, MAKELPARAM(pt.x + 16, pt.y + 16));
    SendMessageA(menutip, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti);
}

// The keys below are the tree's own, the menu is there to say they exist. Delete has none,
// so the menu is the only way to reach it.
void createeditmenu(HWND hDlg) {
    HMENU edit = CreatePopupMenu();
    AppendMenuA(edit, MF_STRING, MENU_SELECTALL, "Select &All\tCtrl+A");
    AppendMenuA(edit, MF_SEPARATOR, 0, NULL);
    AppendMenuA(edit, MF_STRING, MENU_APPLYTAG, "Apply &Tag to Nodes\tT");
    AppendMenuA(edit, MF_STRING, MENU_OVERRIDE, "Manual &Override...\tCtrl+C");
    AppendMenuA(edit, MF_STRING, MENU_HIDE, "&Hide\tCtrl+H");
    AppendMenuA(edit, MF_STRING, MENU_UNHIDE, "&Unhide\tCtrl+U");
    AppendMenuA(edit, MF_SEPARATOR, 0, NULL);
    AppendMenuA(edit, MF_STRING, MENU_MERGESUB, "Merge &Substring Siblings\tCtrl+P");
    AppendMenuA(edit, MF_STRING, MENU_MERGE, "&Merge Nodes\tCtrl+M");
    AppendMenuA(edit, MF_SEPARATOR, 0, NULL);
    AppendMenuA(edit, MF_STRING, MENU_DELETE, "&Delete Nodes...");
    HMENU bar = CreateMenu();
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)edit, "&Edit");
    SetMenu(hDlg, bar);
    // The menu takes its height out of the client area, which the template's controls are
    // laid out against, so the window grows by as much.
    RECT wr;
    GetWindowRect(hDlg, &wr);
    MoveWindow(hDlg, wr.left, wr.top, wr.right - wr.left,
               wr.bottom - wr.top + GetSystemMetrics(SM_CYMENU), TRUE);
    menutip = CreateWindowExA(WS_EX_TOPMOST, TOOLTIPS_CLASSA, NULL,
                              WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT,
                              CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hDlg, NULL, hInst,
                              NULL);
    TOOLINFOA ti = {0};
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE;
    ti.hwnd = hDlg;
    ti.uId = (UINT_PTR)hDlg;
    ti.lpszText = (LPSTR)"";
    SendMessageA(menutip, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    // Without a width the tips stay on one line, and these are sentences.
    SendMessageA(menutip, TTM_SETMAXTIPWIDTH, 0, 400);
}

long handleNotify(HWND hWndDlg, int nIDCtrl, LPNMHDR pNMHDR) {
    switch (pNMHDR->code) {
        case NM_CUSTOMDRAW: {
            if (nIDCtrl == IDC_TREE1 || nIDCtrl == IDC_TREE_DAY) {
                LPNMTVCUSTOMDRAW pNMTVCD = (LPNMTVCUSTOMDRAW)pNMHDR;
                HWND hWndTreeView = pNMHDR->hwndFrom;

                return handleCustomDraw(hWndTreeView, pNMTVCD);
            }
            break;
        }
        case TVN_KEYDOWN: {
            // The day tree is a view of the same nodes, editing them happens on the main one.
            if (nIDCtrl != IDC_TREE1) break;
            NMTVKEYDOWN *kd = (NMTVKEYDOWN *)pNMHDR;
            if (GetKeyState(VK_CONTROL) & 0x8000) switch (kd->wVKey) {
                    case 'C': scaleselection(hWndDlg); return TRUE;
                    case 'H': hideselection(hWndDlg); return TRUE;
                    case 'U': unhideselection(hWndDlg); return TRUE;
                    case 'P': mergesubstringsiblings(hWndDlg); return TRUE;
                    case 'M': mergeselection(hWndDlg); return TRUE;
                }
            else if (kd->wVKey == 'T' && ApplyTagToNode(hWndDlg))
                return TRUE;
            break;
        }
        case TVN_SELCHANGED: {
            if (nIDCtrl != IDC_TREE1 || settingtreesel) break;
            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pNMHDR;
            prevselectednode = selectednode;
            selectednode = (node *)pnmtv->itemNew.lParam;
            // A plain click or an arrow key drops everything else that was selected.
            selectonly(selectednode);
            refreshtreeselection();
            break;
        }
        case TVN_ITEMEXPANDED: {
            if (nIDCtrl != IDC_TREE1) break;
            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pNMHDR;
            node *enode = (node *)pnmtv->itemNew.lParam;
            enode->expanded = pnmtv->action != TVE_COLLAPSE;
            break;
        }
        case TVN_GETINFOTIP: {
            NMTVGETINFOTIP *it = (NMTVGETINFOTIP *)pNMHDR;
            TVITEM tvi = {0};
            tvi.mask = TVIF_HANDLE | TVIF_PARAM;
            tvi.hItem = it->hItem;
            TreeView_GetItem(treeview, &tvi);
            if (tvi.lParam) {
                node *n = (node *)tvi.lParam;
                String s;
                n->formatstats(s);
                strncpy(it->pszText, s.c_str(), it->cchTextMax);
            } else {
                strcpy(it->pszText, "test");
            }
            break;
        }
        case LVN_BEGINLABELEDIT: {
            taglistedit = ListView_GetEditControl(taglist);
            break;
        }
        case LVN_ENDLABELEDIT: {
            // pszText is NULL when the user cancelled the edit rather than confirming it.
            if (!((NMLVDISPINFO *)pNMHDR)->item.pszText) break;
            int sel = SendMessage(taglist, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
            if (sel < 0) break;
            getcontroltext(taglistedit, tags[sel].name, sizeof(tags[sel].name));
            listitem(taglist, tags[sel].name, sel, sel, LVM_SETITEMA);
            break;
        }
        case DTN_DATETIMECHANGE: {
            SYSTEMTIME start, end;
            SendMessageA(startingdatepicker, DTM_GETSYSTEMTIME, 0, (LPARAM)&start);
            SendMessageA(endingdatepicker, DTM_GETSYSTEMTIME, 0, (LPARAM)&end);
            starttime = dayordering(start);
            endtime = dayordering(end);
            rendertree(hWndDlg);
            break;
        }
        case TCN_SELCHANGE: {
            showtab(hWndDlg, TabCtrl_GetCurSel(tabctrl));
            // The control the user just hid would otherwise keep the focus.
            SetFocus(tabctrl);
            break;
        }
    }
    return 0;
}

void setdaterangecontrols(HWND hDlg) {
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

INT_PTR CALLBACK Stats(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG: {
            statsdialog = hDlg;
            filterontag = -1;
            selectednode = prevselectednode = NULL;
            selectonly(NULL);
            filterstrcontents[0] = 0;
            treeview = GetDlgItem(hDlg, IDC_TREE1);
            treeviewproc =
                (WNDPROC)SetWindowLongPtr(treeview, GWLP_WNDPROC, (LONG_PTR)TreeViewProc);
            createeditmenu(hDlg);
            periodlabel = GetDlgItem(hDlg, IDC_STATICHO);
            periodcombo = GetDlgItem(hDlg, IDC_COMBO2);
            normalizedcheck = GetDlgItem(hDlg, IDC_CHECK_NORMALIZED);
            tabctrl = GetDlgItem(hDlg, IDC_TAB1);
            TCITEMA tci;
            tci.mask = TCIF_TEXT;
            tci.pszText = "Statistics";
            SendMessageA(tabctrl, TCM_INSERTITEMA, TAB_STATISTICS, (LPARAM)&tci);
            tci.pszText = "Time View";
            SendMessageA(tabctrl, TCM_INSERTITEMA, TAB_TIME, (LPARAM)&tci);
            RECT tabrect;
            GetWindowRect(tabctrl, &tabrect);
            tabstripheight = tabrect.bottom - tabrect.top;
            // The height from the template is only a guess, the tab row itself must fit.
            RECT tabpage = {0, 0, 100, 100};
            TabCtrl_AdjustRect(tabctrl, FALSE, &tabpage);
            if (tabpage.top > tabstripheight) tabstripheight = tabpage.top;
            timegraphscroll = 0;
            selectedday = -1;
            selectedbar = -1;
            graphperiod = PERIOD_DAY;
            loop(i, PERIOD_NUM) SendMessageA(periodcombo, CB_ADDSTRING, 0, (LPARAM)periodnames[i]);
            SendMessageA(periodcombo, CB_SETCURSEL, graphperiod, 0);
            graphnormalized = false;
            CheckDlgButton(hDlg, IDC_CHECK_NORMALIZED, BST_UNCHECKED);
            if (!barselbrush) barselbrush = CreateSolidBrush(0xE8E8E8);
            timegraph = CreateWindowExA(0, timegraphclass, "", WS_CHILD | WS_HSCROLL | WS_TABSTOP,
                                        0, 0, 0, 0, hDlg, NULL, hInst, NULL);
            if (!timegraph) panic("PT: Cannot create time graph window");
            periodtreeview =
                CreateWindowExA(WS_EX_CLIENTEDGE, WC_TREEVIEWA, "",
                                WS_CHILD | WS_TABSTOP | WS_HSCROLL | TVS_HASBUTTONS | TVS_HASLINES |
                                    TVS_LINESATROOT | TVS_DISABLEDRAGDROP | TVS_SHOWSELALWAYS,
                                0, 0, 0, 0, hDlg, (HMENU)IDC_TREE_DAY, hInst, NULL);
            if (!periodtreeview) panic("PT: Cannot create period tree window");
            tagtimelist = CreateWindowExA(
                WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHAREIMAGELISTS |
                    LVS_NOSORTHEADER,
                0, 0, 0, 0, hDlg, (HMENU)IDC_LIST_TAGTIME, hInst, NULL);
            if (!tagtimelist) panic("PT: Cannot create tag time list window");
            LVCOLUMNA lvc = {0};
            lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            lvc.pszText = "Tag";
            lvc.cx = 100;
            SendMessageA(tagtimelist, LVM_INSERTCOLUMNA, 0, (LPARAM)&lvc);
            lvc.pszText = "Time";
            lvc.cx = 70;
            lvc.iSubItem = 1;
            SendMessageA(tagtimelist, LVM_INSERTCOLUMNA, 1, (LPARAM)&lvc);
            // Controls made outside the dialog template don't get its font.
            SendMessageA(periodtreeview, WM_SETFONT, SendMessageA(hDlg, WM_GETFONT, 0, 0), TRUE);
            SendMessageA(tagtimelist, WM_SETFONT, SendMessageA(hDlg, WM_GETFONT, 0, 0), TRUE);
            endtime = now();
            rendertree(hDlg);
            taglist = GetDlgItem(hDlg, IDC_LIST1);
            if (!tagimages) {
                tagimages = ImageList_Create(bmsize, bmsize, ILC_COLORDDB, MAXTAGS, 0);
                HDC hdc = GetDC(NULL);
                HDC bitmapdc = CreateCompatibleDC(hdc);
                HBITMAP bitmap = CreateCompatibleBitmap(hdc, bmsize, bmsize * MAXTAGS);
                loop(i, MAXTAGS) {
                    HBITMAP old = (HBITMAP)SelectObject(bitmapdc, bitmap);
                    loop(x, bmsize) loop(y, bmsize) SetPixel(bitmapdc, x, y, tags[i].color);
                    SelectObject(bitmapdc,
                                 old);  // seems to be required to flush the drawing commands
                    ImageList_Add(tagimages, bitmap, NULL);
                }
                DeleteObject(bitmap);
                DeleteDC(bitmapdc);
                ReleaseDC(NULL, hdc);
            }
            ListView_SetImageList(taglist, tagimages, LVSIL_SMALL);
            // Sharing it is what LVS_SHAREIMAGELISTS on both of them is for.
            ListView_SetImageList(tagtimelist, tagimages, LVSIL_SMALL);
            loop(i, MAXTAGS) listitem(taglist, tags[i].name, i, i, LVM_INSERTITEMA);
            foldslider = GetDlgItem(hDlg, IDC_SLIDER1);
            SendMessageA(foldslider, TBM_SETRANGE, FALSE, (LPARAM)MAKELONG(1, 5));
            SendMessageA(foldslider, TBM_SETPOS, TRUE, foldlevel);
            minfilter.seteditbox(hDlg);
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
            SendMessageA(hDlg, WM_SETICON, 0,
                         (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_PROCRASTITRACKER)));
            showtab(hDlg, TAB_STATISTICS);
            // WM_SIZE arrives before the controls it needs exist, so lay out here too.
            layoutstats(hDlg);
            // Of a range that doesn't fit, the most recent days are the interesting ones.
            SendMessageA(timegraph, WM_HSCROLL, SB_RIGHT, 0);
            return (INT_PTR)TRUE;
        }
        case WM_DESTROY:
            statsdialog = NULL;
            menutip = NULL;
            tabctrl = NULL;
            timegraph = NULL;
            periodcombo = NULL;
            normalizedcheck = NULL;
            periodlabel = NULL;
            periodtreeview = NULL;
            tagtimelist = NULL;
            break;
        case WM_SIZING: {
            int min_x = 400;
            int min_y = 300;
            LPRECT r = (LPRECT)lParam;
            int ux = -1, uy = -1, wx = r->right - r->left, wy = r->bottom - r->top;
            if (wy < min_y) uy = min_y;
            if (wx < min_x) ux = min_x;
            if (uy != -1) {
                if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT) {
                    r->top = r->bottom - uy;
                } else {
                    r->bottom = r->top + uy;
                }
            }
            if (ux != -1) {
                if (wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT) {
                    r->left = r->right - ux;
                } else {
                    r->right = r->left + ux;
                }
            }
            return TRUE;
        }
        case WM_SIZE: {
            if (timegraph) layoutstats(hDlg);
            break;
        }
        case WM_LBUTTONUP: {
            break;
        }
        case WM_MOUSEWHEEL: {
            break;
        }
        case WM_DRAWITEM: {
            break;
        }
        case WM_KEYUP: {
            break;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDCANCEL: {
                    EndDialog(hDlg, LOWORD(wParam));
                    return (INT_PTR)TRUE;
                }
                case IDC_EDIT2: {
                    if (minfilter.handleeditbox(IDC_EDIT2)) rendertree(hDlg);
                    return (INT_PTR)TRUE;
                }
                case IDC_COMBO1: {
                    if (HIWORD(wParam) == CBN_SELENDOK) {
                        int sel = SendMessage(quickcombo, CB_GETCURSEL, 0, 0);
                        if (sel != CB_ERR) {
                            endtime = now();
                            SYSTEMTIME st;
                            GetLocalTime(&st);
                            switch (sel) {
                                case 0: starttime = endtime; break;  //"Today");
                                case 1:
                                    endtime = dayoffset(endtime, -1);
                                    starttime = endtime;
                                    break;  //"Yesterday");
                                case 2:
                                    // On a Monday this deliberately goes back a whole week.
                                    starttime = dayoffset(
                                        endtime,
                                        -(st.wDayOfWeek == 1 ? 7 : (st.wDayOfWeek + 6) % 7));
                                    break;  //"Since Monday Morning");
                                case 3:
                                    starttime = dayoffset(endtime, -6);
                                    break;  //"Last 7 Days");
                                case 4:
                                    starttime = dayoffset(endtime, -13);
                                    break;  //"Last 14 Days");
                                case 5:
                                    starttime = dayoffset(endtime, -(st.wDay - 1));
                                    break;  //"Month To Date");
                                case 6:
                                    starttime = dayoffset(endtime, -30);
                                    break;  //"30 Days");
                                case 7:
                                    starttime = dayoffset(endtime, -90);
                                    break;  //"90 Days");
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
                            rendertree(hDlg);
                            return (INT_PTR)TRUE;
                        }
                    }
                    break;
                }
                case IDC_CHECK_NORMALIZED: {
                    setgraphnormalized(IsDlgButtonChecked(hDlg, IDC_CHECK_NORMALIZED) != 0);
                    return (INT_PTR)TRUE;
                }
                case IDC_COMBO2: {
                    if (HIWORD(wParam) == CBN_SELENDOK) {
                        int sel = SendMessage(periodcombo, CB_GETCURSEL, 0, 0);
                        if (sel != CB_ERR) {
                            setgraphperiod(sel);
                            return (INT_PTR)TRUE;
                        }
                    }
                    break;
                }
                case IDC_EDIT8: {
                    String old(filterstrcontents);
                    getcontroltext(filterstr, filterstrcontents, 100);
                    String cur(filterstrcontents);
                    if (!(old == cur)) {
                        rendertree(hDlg);
                        return (INT_PTR)TRUE;
                    }
                    break;
                }
                case IDC_BUTTON2:
                case MENU_APPLYTAG: {
                    if (ApplyTagToNode(hDlg)) return (INT_PTR)TRUE;
                    break;
                }
                case MENU_SELECTALL: {
                    selectalltreeitems();
                    return (INT_PTR)TRUE;
                }
                case MENU_OVERRIDE: {
                    scaleselection(hDlg);
                    return (INT_PTR)TRUE;
                }
                case MENU_HIDE: {
                    hideselection(hDlg);
                    return (INT_PTR)TRUE;
                }
                case MENU_UNHIDE: {
                    unhideselection(hDlg);
                    return (INT_PTR)TRUE;
                }
                case MENU_MERGESUB: {
                    mergesubstringsiblings(hDlg);
                    return (INT_PTR)TRUE;
                }
                case MENU_MERGE: {
                    mergeselection(hDlg);
                    return (INT_PTR)TRUE;
                }
                case MENU_DELETE: {
                    deleteselection(hDlg);
                    return (INT_PTR)TRUE;
                }
                case IDC_BUTTON3: {
                    int sel = SendMessage(taglist, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
                    if (sel < 0) break;
                    SetFocus(taglist);
                    ListView_EditLabel(taglist, sel);
                    break;
                }
                case IDC_CHECK1: {
                    int sel = SendMessage(taglist, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
                    if (sel < 0) break;
                    // int sel = SendMessageA(tagdrop, CB_GETCURSEL, 0, 0);
                    if (IsDlgButtonChecked(hDlg, IDC_CHECK1) && sel != CB_ERR)
                        filterontag = sel;
                    else
                        filterontag = -1;
                    rendertree(hDlg);
                    return (INT_PTR)TRUE;
                }
            }
            break;
        }
        case WM_MENUSELECT: {
            const char *tip = NULL;
            // No item is highlighted while a popup is, and none at all once the menu closes.
            if (lParam && !(HIWORD(wParam) & (MF_POPUP | MF_SEPARATOR)))
                tip = menuitemtip(LOWORD(wParam));
            showmenutip(tip);
            return (INT_PTR)TRUE;
        }
        case WM_EXITMENULOOP: {
            showmenutip(NULL);
            return (INT_PTR)TRUE;
        }
        case WM_HSCROLL: {
            int fl = SendMessageA(foldslider, TBM_GETPOS, 0, 0);
            if (fl != foldlevel) {
                foldlevel = fl;
                rendertree(hDlg);
            }
            return (INT_PTR)TRUE;
        }
        case WM_NOTIFY: {
            long lResult = handleNotify(hDlg, (int)wParam, (LPNMHDR)lParam);
            SetWindowLong(hDlg, DWL_MSGRESULT, lResult);
            return (INT_PTR)TRUE;
        } break;
    }
    return (INT_PTR)FALSE;
}
