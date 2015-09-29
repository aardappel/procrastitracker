
struct tagstat
{
    DWORD seconds[MAXTAGS];
    DWORD total;

    tagstat() : total(0) { memset(seconds, 0, sizeof(seconds)); }
    void add(tagstat &o) { loop(i, MAXTAGS) seconds[i] += o.seconds[i]; }
    bool deviates(int total)
    {
        loop(i, MAXTAGS) if (seconds[i] && seconds[i] != total) return true;
        return false;
    }
    void sum() { loop(i, MAXTAGS) total += seconds[i]; }
};

Vector<tagstat> daystats;

struct node : SlabAllocated<node>
{
    node *parent;
    char *nname;
    Hashtable<node *> *ht;
    node *onechild;
    lday *last;
    int tag;
    daydata accum;
    tagstat *ts;
    bool hidden;
    bool expanded;
    bool instrfilter;

    node(char *_name, node *_p)
        : ht(NULL),
          nname(strpool.add(_name)),
          onechild(NULL),
          last(NULL),
          tag(0),
          parent(_p),
          ts(NULL),
          hidden(false),
          expanded(false),
          instrfilter(true)
    {
    }

    ~node()
    {
        DELETEP(onechild);
        DELETEP(ht);
        DELETEP(last);
        DELETEP(ts);
    }

    void remove(node *o)
    {
        if (onechild == o)
            onechild = NULL;
        else if (ht)
            ht->remove(o->nname);
        delete o;
    }

    node *treeparent()
    {
        node *n = parent;
        while (n && n->onechild) n = n->parent;
        return n ? n : this;
    }

    node *firstinchain()
    {
        node *n = this;
        while (n->parent && n->parent->onechild) n = n->parent;
        return n;
    }

    void clearhidden()
    {
        hidden = false;
        if (onechild)
            onechild->clearhidden();
        else if (ht)
        {
            ht->resetiter();
            while (ht->validiter()) ht->nextiter()->clearhidden();
        }
    }

    int gettag()
    {
        node *n = this;
        while (!n->tag && n->parent) n = n->parent;
        return n->tag;
    }

    node *addnode(node *n)
    {
        (*ht)[n->nname] = n;
        return n;
    }

    node *add(char *subname)
    {
        if (!ht) {
            if (!onechild) return onechild = new node(subname, this);
            if (strcmp(onechild->nname, subname) == 0) return onechild;
            ht = new Hashtable<node *>(4);
            addnode(onechild);
            onechild = NULL;
        }
        node **np = ht->find(subname);
        return np ? *np : addnode(new node(subname, this));
    }

    int numdays()
    {
        int n = 0;
        for (lday *d = last; d; d = d->next) n++;
        return n;
    }

    void hit(SYSTEMTIME &st, DWORD idletime, DWORD awaysecs)
    {
        if (last) {
            if (last->nday == dayordering(st)) {
                last->hit(idletime, awaysecs);
                return;
            }
        }
        last = new lday(last);
        last->nday = dayordering(st);
        last->nminute = minuteordering(st);
        last->hit(idletime, awaysecs);
    }

    void formatstats(String &s) { accum.formatstats(s, treeparent()->accum.seconds); }
    void print(FILE *f, bool filtered)
    {
        if (hidden) return;

        fprintf(f, "%s", nname);
        if (onechild) {
            if (!filtered || onechild->accum.seconds) {
                fprintf(f, " - ");
                onechild->print(f, filtered);
            }
            return;
        }
        String s;
        formatstats(s);
        fprintf(f, "<font size=-2> - %s</font>", s.c_str());
        if (ht) {
            int num = numnotfiltered(filtered);
            if (num) {
                Vector<node *> v;
                ht->getelements(v);
                v.sort(nodesorter);
                fprintf(f, "<table border=0 cellspacing=0 cellpadding=3>\n");
                loopv(i, v)
                {
                    if (!filtered || v[i]->accum.seconds) {
                        String s;
                        v[i]->accum.format(s, 0);
                        fprintf(f, "<tr><td align=right valign=top><strong>%s</strong></td><td>", s.c_str());
                        v[i]->print(f, filtered);
                        fprintf(f, "</tr></td>\n");
                    }
                }
                fprintf(f, "</table>\n");
                v.setsize_nd(0);
            }
        }
    }

    int numnotfiltered(bool filtered)
    {
        int num = 0;
        ht->resetiter();
        while (ht->validiter()) {
            node *n = ht->nextiter();
            if (!filtered || n->accum.seconds) num++;
        }
        return num;
    }

    void save(gzFile f, bool filtered)
    {
        gzwrite(f, nname, (int)strlen(nname) + 1);
        wint(f, tag);
        gzputc(f, hidden);

        if (filtered)
        {
            Vector<lday *> infilter;
            if (node_is_in_filter()) {
                for (lday *d = last; d; d = d->next) {
                    if (day_is_in_filter(d->nday)) {
                        infilter.push(d);
                    }
                }
            }
            wint(f, infilter.size());
            for (int i = 0; i < infilter.size(); i++) infilter[i]->save(f);
            infilter.setsize_nd(0);
        }
        else
        {
            wint(f, numdays());
            for (lday *d = last; d; d = d->next) d->save(f);
        }

        if (onechild && (!filtered || onechild->accum.seconds)) {
            wint(f, 1);
            onechild->save(f, filtered);
            return;
        }

        if (ht) {
            int num = numnotfiltered(filtered);
            if (num) {
                wint(f, num);
                ht->resetiter();
                while (ht->validiter()) {
                    node *n = ht->nextiter();
                    if (!filtered || n->accum.seconds) n->save(f, filtered);
                }
                return;
            }
        }

        wint(f, 0);
    }

    bool load(gzFile f, int version, bool merge)
    {
        // FF: struct NODE {

        // FF: char *name (null terminated bytes)
        nname = rstr(f);

        if (version <= 1) rint(f);

        // FF: int tagindex
        if (version >= 3) tag = rint(f);

        // FF: char ishidden
        if (version >= 7) hidden = gzgetc_s(f) != 0;

        // FF: int numberofdays
        int numdays = rint(f);

        if (numdays) {
            lday **dp = &last;
            loop(i, numdays)
            {
                *dp = new lday(NULL);
                // FF: DAY days[numberofdays]
                (*dp)->load(f, version);
                dp = &(*dp)->next;
            }
        }

        // FF: int numchildren
        int numchildren = rint(f);
        if (numchildren) {
            if (numchildren == 1) {
                onechild = new node("", this);
                if (onechild->load(f, version, merge)) {
                    DELETEP(onechild);
                    numchildren = 0;
                }
            }
            else
            {
                int bits = int(logf(numchildren) / logf(2)) + 2;
                // FF: NODE children[numchildren]
                loop(i, numchildren)
                {
                    node *n = new node("", this);
                    if (n->load(f, version, merge)) {
                        DELETEP(n);
                    }
                    else
                    {
                        if (!ht) ht = new Hashtable<node *>(bits);
                        addnode(n);
                    }
                }
                numchildren = ht ? ht->numelems : 0;
            }
        }

        // cull this node if possible
        if (numchildren) return false;
        if (!parent) return false;
        if (!last) return true;

        for (lday **d = &last; *d;) {
            if ((*d)->seconds < prefs[PREF_CULL].ival) {
                lday *dc = *d;
                *d = dc->next;
                dc->next = NULL;
                if (parent->last) {
                    for (lday *pd = parent->last; pd; pd = pd->next) {
                        if (dc->nday == pd->nday) {
                            pd->accumulate(*dc);
                            DELETEP(dc);
                            goto found;
                        }
                    }
                }
                parent->addtotail(dc);
            found:;
            }
            else
            {
                d = &(*d)->next;
            }
        }

        return last == NULL;

        // FF: }
    }

    void addtotail(lday *o)
    {
        lday **dp = &last;
        while (*dp) dp = &(*dp)->next;
        *dp = o;
    }

    bool node_is_in_filter() { return last && (filterontag < 0 || filterontag == gettag()) && instrfilter; }
    bool day_is_in_filter(int o) { return o >= starttime && o <= endtime; }

    void accumulate(daydata &pd, tagstat &pts)
    {
        accum = daydata();
        tagstat tts;
        if (node_is_in_filter()) {
            for (lday *d = last; d; d = d->next) {
                int o = d->nday;
                if (day_is_in_filter(o)) {
                    accum.accumulate(*d);
                    daystats[o - starttime].seconds[gettag()] += d->seconds;
                }
            }
            tts.seconds[gettag()] += accum.seconds;
        }
        if (onechild)
            onechild->accumulate(accum, tts);
        else if (ht)
        {
            ht->resetiter();
            while (ht->validiter()) ht->nextiter()->accumulate(accum, tts);
        }
        if (tts.deviates(accum.seconds) && !ts) ts = new tagstat;
        if (ts) *ts = tts;
        pts.add(tts);
        pd.accumulate(accum);
    }

    void treeview(int depth, HWND hWnd, HTREEITEM parent, HTREEITEM after, int timelevel = 0, char *concat = "")
    {
        if (hidden) return;

        if (!accum.seconds || accum.seconds / 60 < minfilter.ival) return;

        String full(concat);
        if (*concat) full.Cat(" - ");
        full.Cat(nname);

        if (onechild && !last) return onechild->treeview(depth, hWnd, parent, after, timelevel, full);

        String s;
        accum.format(s, timelevel);
        s.Cat(" - ");
        s.Cat(full);

        const int MAXSTR = 1000;
        static wchar_t us[MAXSTR];
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, us, MAXSTR);
        us[MAXSTR - 1] = 0;

        TV_INSERTSTRUCTW tvinsert;
        tvinsert.hParent = parent;
        tvinsert.hInsertAfter = after;
        tvinsert.item.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
        tvinsert.item.pszText = us;
        tvinsert.item.lParam = (LPARAM) this;
        tvinsert.item.state = (depth < (int)foldlevel && ht) || expanded ? TVIS_EXPANDED : 0;
        tvinsert.item.stateMask = -1;
        HTREEITEM thisone = (HTREEITEM)SendDlgItemMessageW(hWnd, IDC_TREE1, TVM_INSERTITEMW, 0, (LPARAM)&tvinsert);

        if (onechild) {
            onechild->treeview(depth + 1, hWnd, thisone, TVI_LAST);
        }
        else if (ht)
        {
            Vector<node *> v;
            ht->getelements(v);
            v.sort(nodesorter);
            int timelevel = 0;
            if (v[0]->accum.seconds > 60) timelevel++;
            if (v[0]->accum.seconds > 60 * 60) timelevel++;
            if (parent == NULL) maxsecondsforbargraph = v[0]->accum.seconds;
            loopv(i, v) v[i]->treeview(depth + 1, hWnd, thisone, TVI_LAST, timelevel);
            v.setsize_nd(0);
        }
    }

    static int nodesorter(node **a, node **b)
    {
        return (*a)->accum.seconds < (*b)->accum.seconds ? 1 : (*a)->accum.seconds > (*b)->accum.seconds ? -1 : 0;
    }

    void merge(node &o)
    {
        for (lday *od = o.last; od; od = od->next) {
            for (lday *d = last; d; d = d->next) {
                if (d->nday == od->nday) {
                    d->accumulate(*od);
                    goto daydone;
                }
            }
            lday *nd = new lday(NULL);
            *(daydata *)nd = *(daydata *)od;
            addtotail(nd);
        daydone:;
        }

        if (o.onechild)
            add(o.onechild->nname)->merge(*o.onechild);
        else if (o.ht)
        {
            o.ht->resetiter();
            while (o.ht->validiter()) {
                node *n = o.ht->nextiter();
                add(n->nname)->merge(*n);
            }
        }
    }

    void mergallsubstring()
    {
        if (!parent || !parent->ht) return;
        parent->ht->resetiter();
        while (parent->ht->validiter()) {
            node *n = parent->ht->nextiter();
            if (n != this && strstr(n->nname, nname)) {
                merge(*n);
                parent->remove(n);
            }
        }
    }

    void collectstats()
    {
        statnodes++;
        statdays += numdays();
        if (!ht && !onechild) statleaf++;
        if (onechild) {
            statone++;
            onechild->collectstats();
        }
        else if (ht)
        {
            statht++;
            ht->resetiter();
            while (ht->validiter()) ht->nextiter()->collectstats();
        }
    }

    void checkstrfilter(bool force)
    {
        instrfilter = force || stristr(nname, filterstrcontents);

        if (onechild)
            onechild->checkstrfilter(instrfilter);
        else if (ht)
        {
            ht->resetiter();
            while (ht->validiter()) ht->nextiter()->checkstrfilter(instrfilter);
        }
    }
};
