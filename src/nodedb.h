
void save(bool filtered = false, char *givenfilename = NULL) {
    static int firstsave = TRUE;
    gzFile f = gzopen(givenfilename ? givenfilename : databasetemp, "wb1h");
    if (!f) panic("PT: could not open database file for writing");
    wint(f, FILE_FORMAT_VERSION);
    wint(f, 'PTFF');
    wint(f, MAXTAGS);
    loop(i, MAXTAGS) {
        gzwrite(f, tags[i].name, sizeof(tags[i].name));
        wint(f, tags[i].color);
    }
    wint(f, minfilter.ival * 60);
    wint(f, foldlevel);
    loop(i, NUM_PREFS) wint(f, prefs[i].ival);
    root->save(f, filtered);
    gzclose(f);
    if (!givenfilename) {
        lastsavetime = GetTickCount();
        // only delete previous backups if we have a database
        if (GetFileAttributesA(databasemain) != INVALID_FILE_ATTRIBUTES) {
            if (firstsave) {
                SYSTEMTIME st;
                GetLocalTime(&st);
                char lrun[MAX_PATH];
                sprintf_s(lrun, MAX_PATH, "%sdb_BACKUP_%d_%02d_%02d.PT", databaseroot, st.wYear,
                          st.wMonth, st.wDay);
                DeleteFileA(lrun);
                CopyFileA(databasemain, lrun, FALSE);  // backup last saved of last run once per day
                firstsave = FALSE;
            }
            DeleteFileA(databaseback);
            MoveFileA(databasemain, databaseback);  // backup last saved
        }
        MoveFileA(databasetemp, databasemain);
    }
}

void load(node *root, char *fn, bool merge) {
    // FF: struct DATABASE {
    gzFile f = gzopen(fn, "rb");
    if (!f) return;  // inform the user if its not his first run?
    // FF: int version (== 10, as of v1.6, format may be different if other version)
    int version = rint(f);
    if (version > FILE_FORMAT_VERSION) panic("PT: trying to load db from newer version");
    // FF: int magic (== 'PTFF' on x86)
    if (version >= 6 && rint(f) != 'PTFF') panic("PT: not a PT database file");
    if (version >= 4) {
        // don't overwrite global data
        if (merge) {
            int ntags = rint(f);
            uint bytes = ntags * (sizeof(tag().name) + sizeof(int));  // tags
            bytes += version < 6 ? 10 : sizeof(int);                  // minfilter
            bytes += sizeof(int);                                     // foldlevel
            if (version >= 6) {                                       // prefs
                bytes += 5 * sizeof(int);
                if (version >= 10) bytes += sizeof(int);
            }
            loop(i, (int)bytes) gzgetc_s(f);
        } else {
            // FF: int numtags
            int ntags = rint(f);
            if (ntags > MAXTAGS) panic("PT: wrong number of tags in file");
            loop(i, ntags) {
                // FF: struct { char name[32]; int color; } tags[numtags]
                gzread_s(f, tags[i].name, sizeof(tags[i].name));
                int col = rint(f);
                tags[i].color = col;
            }
            // FF: int minfilter
            if (version < 6) {
                loop(i, 10) gzgetc_s(f);
            } else
                minfilter.ival = rint(f) / 60;
            // FF: int foldlevel
            foldlevel = rint(f);
            ASSERT(NUM_PREFS == 6);
            if (version >= 6) {
                // FF: int prefs[6] (see advanced prefs window)
                loop(i, 5) prefs[i].ival = rint(f);
                if (version >= 10) prefs[5].ival = rint(f);
            }
        }
        if (version == 8) loop(i, 24) gzgetc_s(f);
    }
    // FF: NODE root
    root->nname = "";
    root->load(f, version, merge);
    gzclose(f);
    // FF: }
}

void exporthtml(char *fn) {
    FILE *f = fopen(fn, "w");
    if (f) {
        fprintf(f, "<HTML><HEAD><TITLE>Procrastitracker export</TITLE></HEAD><BODY>");
        root->print(f, true);
        fprintf(f, "</BODY></HTML>");
        fclose(f);
    }
}

void mergedb(char *fn) {
    node *dbr = new node("(root)", NULL);
    load(dbr, fn, true);
    root->merge(*dbr);
    delete dbr;
}

static char *seperators[] = {" - ", " | ", " : ", " > ", "\\\\", "\\", "//", "/", NULL};

void addtodatabase(char *elements, SYSTEMTIME &st, DWORD idletime, DWORD awaysecs) {
    node *n = root;
    for (char *rest = elements, *head; *rest;) {
        head = rest;
        char *sep = NULL;
        int sepl = 0;
        for (char **seps = seperators; *seps; seps++) {
            char *sepn = strstr(rest, *seps);
            if (sepn && (!sep || sepn < sep)) {
                sep = sepn;
                sepl = strlen(*seps);
            }
        }
        if (sep) {
            rest = sep + sepl;
            *sep = 0;
        } else {
            rest = "";
        }
        if (*head) {
            if (head[0] == '[') head++;
            for (;;) {
                size_t l = strlen(head);
                if (l && (head[l - 1] == '*' || head[l - 1] == ']'))
                    head[l - 1] = 0;
                else
                    break;
            }
            if (*head) n = n->add(head);
        }
    }
    n->hit(st, idletime, awaysecs);
}
