#ifdef _DEBUG
    //#define PASSTHRUALLOC
#endif

// very simple doubly linked list
// std::list doesn't have a node to inherit from, so causes 2 allocations rather than 1 for object hierarchies

//#define DLNode_INIT     // due to SlabAlloc

struct DLNode
{
	DLNode *prev, *next;

	DLNode() : prev(NULL), next(NULL) {}   

	#ifdef DLNode_INIT
	virtual
	#endif
	~DLNode()
	{
		#ifdef DLNode_INIT
		if(Connected()) Remove();
		#endif
	}

	bool Connected() { return next && prev; }

	void Remove()
	{
		#ifdef DLNode_INIT
		assert(Connected());
		#endif

		prev->next = next;
		next->prev = prev;

		#ifdef DLNode_INIT
		next = prev = NULL;
		#endif
	}

	void InsertAfterThis(DLNode *o)
	{
		#ifdef DLNode_INIT
		assert(Connected() && !o->Connected());
		#endif

		o->next = next;
		o->prev = this;
		next->prev = o;
		next = o;
	}

	void InsertBeforeThis(DLNode *o)
	{
		#ifdef DLNode_INIT
		assert(Connected() && !o->Connected());
		#endif

		o->prev = prev;
		o->next = this;
		prev->next = o;
		prev = o;
	}
};

struct DLList : DLNode
{
	DLList() { next = prev = this; }

	bool Empty() { return next==this; }

	DLNode *Get()
	{
		assert(!Empty());
		DLNode *r = next;
		r->Remove();
		return r;
	}
};

#define loopdllistother(L, T, n)   for(T *n = (T *)(L)->next;                    n!=(T *)(L);          n = (T *)n->next)    // safe Remove on not self
#define loopdllist(L, T, n)        for(T *n = (T *)(L)->next, *p = (T *)n->next; n!=(T *)(L); (n = p),(p = (T *)n->next))   // safe Remove on self
#define loopdllistreverse(L, T, n) for(T *n = (T *)(L)->prev, *p = (T *)n->prev; n!=(T *)(L); (n = p),(p = (T *)n->prev))

class SlabAlloc
{
    // tweakables:
    enum { MAXBUCKETS = 32 };   // must be ^2.
                                // lower means more blocks have to go thru the traditional allocator (slower)
                                // higher means you may get pages with only few allocs of that unique size (memory wasted)
    enum { PAGESATONCE = 101 }; // depends on how much you want to take from the OS at once: PAGEATONCE*PAGESIZE
                                // You will waste 1 page to alignment
                                // with MAXBUCKETS at 32 on a 32bit system, PAGESIZE is 2048, so this is 202k

    // derived:
    enum { PTRBITS = sizeof(char *)==4 ? 2 : 3 };  // "64bit should be enough for everyone"
                                                   // everything is twice as big on 64bit: alignment, memory blocks, and pages
    enum { ALIGNBITS = PTRBITS+1 };                // must fit 2 pointers in smallest block for doubly linked list
    enum { ALIGN = 1<<ALIGNBITS };
    enum { ALIGNMASK = ALIGN-1 };

    enum { MAXREUSESIZE = (MAXBUCKETS-1)*ALIGN };

	enum { PAGESIZE = MAXBUCKETS*ALIGN*8 }; // meaning the largest block will fit almost 8 times

    enum { PAGEMASK = (~(PAGESIZE-1)) };
	enum { PAGEBLOCKSIZE = PAGESIZE*PAGESATONCE };

    struct PageHeader : DLNode
    {
        int refc;
        int size;
        char *isfree;
    };

	inline int bucket(int s)
    {
        return (s+ALIGNMASK)>>ALIGNBITS;
    }

	inline PageHeader *ppage(void *p)
    {
        return (PageHeader *)(((size_t)p)&PAGEMASK);
    }

    inline int numobjs(int size) { return (PAGESIZE-sizeof(PageHeader))/size; }

	DLList reuse[MAXBUCKETS];
	DLList freepages, usedpages;
    void **blocks;

    long long stats[MAXBUCKETS];    // FIXME: debug only
    long long statbig;

	void putinbuckets(char *start, char *end, int b, int size)
	{
		for(end -= size; start<=end; start += size)
		{
			reuse[b].InsertAfterThis((DLNode *)start);
		}
	}

	void newpageblocks()
	{
		void **b = (void **)malloc(PAGEBLOCKSIZE+sizeof(void *)); // if we could get page aligned memory here, that would be even better
        assert(b);
        *b = (void *)blocks;
        blocks = b;
        b++;

        char *first = ((char *)ppage(b))+PAGESIZE;
		for(int i = 0; i<PAGESATONCE-1; i++)
		{
			PageHeader *p = (PageHeader *)(first+i*PAGESIZE);
			freepages.InsertAfterThis(p);
		}
        /*
		if(first-b>minimum_useful)          available_memory(b, first-b); 
		if(b-first+PAGESIZE>minimum_useful) available_memory(first+PAGEBLOCKSIZE-PAGESIZE, b-first+PAGESIZE);
        */
	}

	void *newpage(int b)
	{
        assert(b);

		if(freepages.Empty()) newpageblocks();
		PageHeader *page = (PageHeader *)freepages.Get();
        usedpages.InsertAfterThis(page);
		page->refc = 0;
		page->size = b*ALIGN;
        putinbuckets((char *)(page+1), ((char *)page)+PAGESIZE, b, page->size);
		return alloc(page->size);
	}

	void freepage(PageHeader *page, int size)
	{
        for(char *b = (char *)(page+1); b+size<=((char *)page)+PAGESIZE; b += size)
            ((DLNode *)b)->Remove();

        page->Remove();
        freepages.InsertAfterThis(page);
	}

    public:

	SlabAlloc() : blocks(NULL), statbig(0)
	{
		for(int i = 0; i<MAXBUCKETS; i++)
        {
            stats[i] = 0;
        }
	}

    ~SlabAlloc()
    {
        while(blocks)
        {
            void *next = *blocks;
            free(blocks);
            blocks = (void **)next;
        }
    }

	void *alloc(size_t size)
	{
        #ifdef PASSTHRUALLOC
            return malloc(size);
        #else
            if(size>MAXREUSESIZE) { statbig++; return malloc(size); }

		    int b = bucket((int)size);
            #ifdef _DEBUG
                stats[b]++;
            #endif

		    if(reuse[b].Empty()) return newpage(b);

		    DLNode *r = reuse[b].Get();

		    ppage(r)->refc++;

		    return r;
        #endif
	}

	void dealloc(void *p, size_t size)
	{
        #ifdef PASSTHRUALLOC
            free(p);
        #else
            #ifdef _DEBUG     
                memset(p, 0xBA, size); 
            #endif

		    if(size>MAXREUSESIZE)
		    {
			    free(p);
		    }
		    else
		    {
			    int b = bucket((int)size);
			    reuse[b].InsertAfterThis((DLNode *)p);

                PageHeader *page = ppage(p);
			    if(!--page->refc) freepage(page, b*ALIGN);
		    }
        #endif
	}

    void *resize(void *p, size_t oldsize, size_t size)
    {
        void *np = alloc(size);
        memcpy(np, p, size>oldsize ? oldsize : size);
        dealloc(p, oldsize);
        return np;
    }

    // versions of the above functions that track size for you

    void *alloc_sized(size_t size)
    {
        size_t *p = (size_t *)alloc(size+sizeof(size_t));
        *p++ = size;  // stores 2 sizes for big objects!
        return p;
    }

    void dealloc_sized(void *p) 
    {
        size_t *t = (size_t *)p;
        size_t size = *--t;	
        size += sizeof(size_t);
        dealloc(t, size);
    }

    void *resize_sized(void *p, size_t size)
    {
        void *np = alloc_sized(size);
        size_t oldsize = ((size_t *)p)[-1];
        memcpy(np, p, size>oldsize ? oldsize : size);
        dealloc_sized(p);
        return np;
    }

	// convenient string allocation

	char *alloc_string(const char *from)
	{
		char *buf = (char *)alloc(strlen(from) + 1);
		strcpy(buf, from);
		return buf;
	}

	void dealloc_string(char *p)
	{
		dealloc(p, strlen(p) + 1);
	}

    // typed helpers

    template<typename T> T *allocobj()
    {
        return (T *)alloc(sizeof(T));
    }

    template<typename T> void destruct(T *obj)
    {
        obj->~T();
        dealloc(obj, sizeof(T));
    }

	void printstats()
	{
		size_t totalwaste = 0;
        long long totalallocs = 0;
		char buf[100];
		for(int i = 0; i<MAXBUCKETS; i++)
		{
			size_t num = 0;
            loopdllist(&reuse[i], DLNode, n) num++;
			if(num || stats[i])
			{
				size_t waste = (i*ALIGN*num+512)/1024;
				totalwaste += waste;
                totalallocs += stats[i];
				sprintf(buf, "bucket %d -> %lu (%lu k), %lld allocs\n", i*ALIGN, num, waste, stats[i]);
                #ifdef WIN32
				::OutputDebugStringA(buf); 
                #endif
			}
		}

        int numfree = 0, numused = 0;
        loopdllist(&freepages, PageHeader, h) numfree++;
        loopdllist(&usedpages, PageHeader, h) numused++;

        sprintf(buf, "totalwaste %lu k, pages %d empty / %d used, %lld total allocs, %lld big allocs\n", totalwaste, numfree, numused, totalallocs, statbig);
        #ifdef WIN32
		::OutputDebugStringA(buf); 
        #endif
	}
};
