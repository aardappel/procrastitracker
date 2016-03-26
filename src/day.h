
const int monthfactor = 32;
const int yearfactor = 512;
const int yearbase = 2000;

int dayordering(SYSTEMTIME &st) { return st.wDay + st.wMonth * monthfactor + (st.wYear - yearbase) * yearfactor; }
int minuteordering(SYSTEMTIME &st) { return st.wHour * 60 + st.wMinute; }
int now()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    return dayordering(st);
}

struct daydata
{
    WORD nday, nminute;
    DWORD seconds, semiidleseconds;
    int key, lmb, rmb, scr;

    daydata() : nday(0), nminute(0), seconds(0), semiidleseconds(0), key(0), lmb(0), rmb(0), scr(0) {}
    void createsystime(SYSTEMTIME &st)
    {
        st.wYear = nday / yearfactor;
        st.wMonth = (nday - st.wYear * yearfactor) / monthfactor;
        st.wDay = nday - st.wYear * yearfactor - st.wMonth * monthfactor;
        st.wYear += yearbase;

        st.wHour = nminute / 60;
        st.wMinute = nminute - st.wHour * 60;

        st.wDayOfWeek = 0;
        st.wMilliseconds = 0;
        st.wSecond = 0;
    }

    void hit(DWORD idletime, DWORD awaysecs)
    {
        if (awaysecs)
        {
            seconds += awaysecs;
        }
        else
        {
            seconds += prefs[PREF_SAMPLE].ival;
            if (idletime > prefs[PREF_SEMIIDLE].ival) semiidleseconds += prefs[PREF_SAMPLE].ival;
            inputhookstats(key, lmb, rmb, scr);
        }
    }

    void accumulate(daydata &o, bool copylastdayminute)
    {
        seconds += o.seconds;
        semiidleseconds += o.semiidleseconds;
        key += o.key;
        lmb += o.lmb;
        rmb += o.rmb;
        scr += o.scr;
        if (copylastdayminute && o.nday && !nday)
        {
            nday = o.nday;
            nminute = o.nminute;
        }
    }

    void format(String &s, int timelevel)
    {
        int secs = seconds;
        int mins = secs / 60;
        secs -= mins * 60;
        int hrs = mins / 60;
        mins -= hrs * 60;
        if (hrs || timelevel > 1) s.FormatCat("%d:", hrs);
        if (mins || timelevel > 0) s.FormatCat("%02d:", mins);
        s.FormatCat("%02d", secs);
    }

    void formatstats(String &s, DWORD parentsec)
    {
        s.FormatCat("%d%% of parent", seconds * 100 / parentsec);
        if (semiidleseconds) s.FormatCat(", %d%% semiidle", semiidleseconds * 100 / seconds);
        if (key) s.FormatCat(", %d keys", key);
        if (lmb) s.FormatCat(", %d lmb", lmb);
        if (rmb) s.FormatCat(", %d rmb", rmb);
        if (scr) s.FormatCat(", %d scrollwheel", scr);
        if (nminute && nday)
        {
            SYSTEMTIME st;
            createsystime(st);
            s.FormatCat(", %d:%02d start on %d-%d-%d", st.wHour, st.wMinute, st.wYear, st.wMonth, st.wDay);
        }
    }

    int print(FILE *f)
    {
        String s;
        format(s, 0);
        SYSTEMTIME st;
        createsystime(st);
        fprintf(f, ": %d-%d-%d: %s [%d key, %d lmb, %d rmb, %d scr]", st.wYear, st.wMonth, st.wDay, s.c_str(), key, lmb,
                rmb, scr);
        return seconds;
    }

    void save(gzFile f) { gzwrite(f, this, sizeof(daydata)); }
    void load(gzFile f, int version)
    {
        // FF: struct DAY {

        if (version < 5) 
        {
            SYSTEMTIME st;
            gzread_s(f, &st, sizeof(SYSTEMTIME));
            nday = dayordering(st);
            nminute = minuteordering(st);
        }
        else
        {
            // FF: unsigned short day (lower 5 bits = day, next 4 bits = month, rest = year counted from 2000)
            // FF: unsigned short firstminuteused (counted from midnight)
            gzread_s(f, &nday, sizeof(WORD) * 2);
        }
        // FF: int activeseconds, semiidleseconds, key, lmb, rmb, scrollwheel
        gzread_s(f, &seconds, sizeof(int) * 6);
        if (nday < starttime) starttime = nday;

        // FF: }
    }
};

struct lday : daydata, SlabAllocated<lday>
{
    lday *next;
    lday(lday *_n) : next(_n) {}
    ~lday() { DELETEP(next); }
};
