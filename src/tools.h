typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

static const float PI = 3.14159265358979323846264338327950288419716939937510f; // :)
static const float RAD = PI/180.0f;

#ifdef _DEBUG
#define ASSERT(c) if(!(c)) { __asm int 3 }
#else
#define ASSERT(c) if(c) {}
#endif

#define loop(i, m) for(int i = 0; i<(m); i++)
#define loopv(i, v) loop(i, (v).size())

#define varargs(v, fmt, body) { va_list v; va_start(v, fmt); body; va_end(v); }  

#define DELETEP(p) if(p) { delete   p; p = NULL; }
#define DELETEA(a) if(a) { delete[] a; a = NULL; }

#define bound(v, a, s, e)  { v += a; if(v>(e)) v = (e); if(v<(s)) v = (s); }       

template<class T> inline void swap(T &a, T &b) { T c = a; a = b; b = c; };

#ifdef WIN32
#pragma warning (3: 4189)       // local variable is initialized but not referenced
#pragma warning (disable: 4244) // conversion from 'int' to 'float', possible loss of data
#pragma warning (disable: 4355) // 'this' : used in base member initializer list
#pragma warning (disable: 4996) // 'strncpy' was declared deprecated
#endif


template <typename T> struct SlabAllocated
{
	void *operator new(size_t size) { assert(size == sizeof(T)); return pool.alloc(size); }
	void operator delete(void *p) { pool.dealloc(p, sizeof(T)); };
};


// from boost, stops a class from being accidental victim to default copy + destruct twice problem
// class that inherits from NonCopyable will work correctly with Vector, but give compile time error with std::vector

class NonCopyable       
{
    NonCopyable(const NonCopyable&);
    const NonCopyable& operator=(const NonCopyable&);

protected:

    NonCopyable() {}
    //virtual ~NonCopyable() {}
};

// helper function for containers below to delete members if they are of pointer type only

template <class X> void DelPtr(X &)   {}
template <class X> void DelPtr(X *&m) { DELETEP(m); }


// replacement for STL vector

// for any T that has a non-trivial destructor, STL requires a correct copy constructor / assignment op
// or otherwise it will free things twice on a reallocate/by value push_back.
// The vector below will behave correctly irrespective of whether a copy constructor is available
// or not and avoids the massive unnecessary (de/re)allocations the STL forces you to.
// The class itself never calls T's copy constructor, however the user of the class is still
// responsable for making sure of correct copying of elements obtained from the vector, or setting NonCopyable

// also automatically deletes pointer members and other neat things

template <class T> class Vector : public NonCopyable
{
    T *buf;
    uint alen, ulen;

    void reallocate(uint n)
    {
        ASSERT(n>ulen);
        T *obuf = buf;
        buf = (T *)malloc(sizeof(T)*(alen = n));        // use malloc instead of new to avoid constructor
        ASSERT(buf);
        if(ulen) memcpy(buf, obuf, ulen*sizeof(T));     // memcpy avoids copy constructor
        if(obuf) free(obuf);
    }

    void destruct(uint i) { buf[i].~T(); DelPtr(buf[i]); }

    public:

    T &push_nocons()
    {
        if(ulen==alen) reallocate(alen*2);
        return buf[ulen++];
    }

    T &operator[](uint i) { ASSERT(i<ulen); return buf[i]; }

    Vector()              : ulen(0), buf(NULL) { reallocate(8); }
    Vector(uint n)        : ulen(0), buf(NULL) { reallocate(n); }
    Vector(uint n, int c) : ulen(0), buf(NULL) { reallocate(n); loop(i, c) push(); }

    ~Vector() { setsize(0); free(buf); }

    T &push(const T &elem) { push_nocons() = elem; return buf[ulen - 1]; }

    T *getbuf()  { return buf; }
    T &last()    { return (*this)[ulen-1]; }
    T &pop()     { return buf[--ulen]; }
    void drop()  { ASSERT(ulen); destruct(--ulen); }
    bool empty() { return ulen==0; }
    int size()   { return ulen; }

    void setsize   (uint i) { while(ulen>i) drop(); }    // explicitly destruct elements
    void setsize_nd(uint i) { ulen = std::min(ulen, i); }    

    void sort(void *cf) { qsort(buf, ulen, sizeof(T), (int (__cdecl *)(const void *, const void *))cf); }
    
    void add_unique(T x)
    {
        loop(i, (int)ulen) if(buf[i]==x) return;
        push() = x;
    }

    void remove(uint i)
    {
        ASSERT(i<ulen);
        destruct(i);
        memmove(buf+i, buf+i+1, sizeof(T)*(ulen---i-1));
    }

    void removeobj(T o)
    {
        loop(i, (int)ulen) if(buf[i]==o) { remove(i); return; }
    }

    T &insert_nc(uint i)
    {
        push_nocons();
        memmove(&buf[i+1], &buf[i], sizeof(T)*(ulen-i-1));
        return buf[i];
    }

    T &insert(uint i)
    {
        T &t = insert_nc(i);
        t = T();
        return t;
    }
};




// simple safe string class

class String : public NonCopyable
{
    static const int smallsize = 32;
    char *buf;
    int ulen, alen;
    char sbuf[smallsize];

    public:

    String()                       : buf(NULL), ulen(0), alen(smallsize) { sbuf[0] = 0; }
    explicit String(const char *s) : buf(NULL), ulen(0), alen(smallsize) { sbuf[0] = 0; Copy(s); }
    ~String() { DELETEP(buf); }

    char *c_str() { return buf ? buf : sbuf; }
    operator char *() { return c_str(); }

    int Len() { return ulen; }

    char *Copy(const char *s) { return Copy(s, (int)strlen(s)); }
    char *Copy(const char *s, int len) { ulen = 0; return Cat(s, len); }
    char *Copy(float f) { return Format("%f", f); }
    char *Copy(int i)   { return Format("%d", i); }
    char *Cat(const char *s) { return Cat(s, (int)strlen(s)); }
    char *Cat(uchar c) { return Cat((char *)&c, 1); }

    char *Cat(const char *s, int len)
    {
        char *b = c_str();
        if(len+ulen>=alen)
        {
            char *nb = new char[alen = ((len+ulen)/smallsize+1)*smallsize];
            if(ulen) strcpy(nb, b);
            if(b!=sbuf) DELETEP(b);
            buf = b = nb;
        }
        strncpy(b+ulen, s, len);
        b[ulen += len] = 0;
        return b;
    }
    
    void Trim(int p) { c_str()[ulen = p] = 0; }

    char *Format(char* fmt, ...) { Clear(); char *r; varargs(v, fmt, r = FormatVA(fmt, v)); return r; }
    char *FormatCat(char* fmt, ...) {       char *r; varargs(v, fmt, r = FormatVA(fmt, v)); return r; }
    char *FormatVA(char* fmt, va_list v)
    {
        char t[1000]; //FIXME ugh. any way to know size ahead of time?
        _vsnprintf(t, 1000, fmt, v);
        return Cat(t);
    }

    void Clear() { Copy(""); }

    char &operator[](int i) { ASSERT(i>=0 && i<=ulen); return c_str()[i]; }

    bool operator==(String &o) { return !strcmp(c_str(), o.c_str()); }
    
    void Lowercase() { loop(i, ulen) c_str()[i] = tolower(c_str()[i]); }
};


inline void OutputDebugF(char* fmt, ...) { String s; varargs(v, fmt, s.FormatVA(fmt, v)); OutputDebugStringA(s); }


// Hashtable that uses strings for keys
// Does not copy keys, client responsible for key memory
// Upon destruction, deletes contained values if they are pointers

template <class T> class Hashtable : public NonCopyable
{
    protected:

	struct chain : SlabAllocated<chain> { chain *next; char *key; T data; };

    int size;
    chain **table;

    public:

    int numelems;

    Hashtable(int nbits = 10)
    {
        this->size = 1<<nbits;
        numelems = 0;
        table = new chain *[size];
        loop(i, size) table[i] = NULL;
    }

    ~Hashtable()
    {
        clear();
        DELETEA(table);
    }

    void clear(bool delptr = true)
    {
        loop(i, size)
        {
            for(chain *d, *c = table[i]; d = c; )
            {
                c = c->next;
                if(delptr) DelPtr(d->data);
                DELETEP(d); 
            }
            table[i] = NULL;
        }
		numelems = 0;
    }
    
    void remove(char *key)
    {
        unsigned int h;
        chain *t = find(key, h);
        if(t)
        {
            for(chain **c = &table[h]; *c; c = &(*c)->next) if(strcmp(key, (*c)->key)==0)
            {
                chain *next = (*c)->next;
                delete (*c);
                *c = next;
				numelems--;
                return;
            }
        }
    }

    T &operator[](char *key)
    {
        unsigned int h;
        chain *old = find(key, h);
        return old ? old->data : insert(key, h)->data;
    }

    T *find(char *key)
    {
        unsigned int h;
		chain *c = find(key, h);
		return c ? &c->data : NULL;
    }
	
    void getelements(Vector<T> &v)
    {
        loop(h, size) for(chain *c = table[h]; c; c = c->next) v.push(c->data);
    }

	int biter;
	chain *citer;

	void resetiter()
	{
		biter = 0;
		citer = table[0];
		advanceiter();
	}

	void advanceiter()
	{
		while (!citer)
		{
			biter++;
			if (biter == size) break;
			citer = table[biter];
		} 
	}

	bool validiter() { return biter < size; }

	T &nextiter()
	{
		T &cur = citer->data;
		citer = citer->next;
		advanceiter();
		return cur;
	}

    protected:

    chain *insert(char *key, unsigned int h)
    {
        chain *c = new chain;
        c->key = key;
        c->next = table[h];
        memset(&c->data, 0, sizeof(T));
        table[h] = c;
        numelems++;
        return c;
    }

    chain *find(char *key, unsigned int &h)
    {
        h = 5381;
        for(int i = 0, k; k = key[i]; i++) h = ((h<<5)+h)^k;    // bernstein k=33 xor
        h = h&(size-1);                                         // primes not much of an advantage
        for(chain *c = table[h]; c; c = c->next) if(strcmp(key,c->key)==0) return c;
        return NULL;
    }
};

struct Nothing {};	// FIXME: this still has a sizeof() of 1, thus costs 8 bytes extra
struct StringPool : public Hashtable<Nothing>
{
	StringPool() : Hashtable(16) {}

    char *add(char *s)
    {
        unsigned int h;
        chain *c = find(s, h);
        if(c) return c->key;
		s = pool.alloc_string(s);
        insert(s, h);
        return s;
    }
};

inline const char *stristr(const char *haystack, const char *needle)
{
	if ( !*needle )
	{
		return haystack;
	}
	char firstneedle = toupper(*needle);
	for ( ; *haystack; ++haystack )
	{
		if ( toupper(*haystack) == firstneedle )
		{
			// Matched starting char -- loop through remaining chars.
			const char *h, *n;
			for ( h = haystack, n = needle; *h && *n; ++h, ++n )
			{
				if ( toupper(*h) != toupper(*n) )
				{
					break;
				}
			}
			if ( !*n ) /* matched all of 'needle' to null termination */
			{
				return haystack; /* return the start of the match */
			}
		}
	}
	return 0;
}

class MTRnd
{
    const static uint N = 624;             
    const static uint M = 397;               
    const static uint K = 0x9908B0DFU;   

    uint hiBit(uint u)  { return u & 0x80000000U; }  
    uint loBit(uint u)  { return u & 0x00000001U; }  
    uint loBits(uint u) { return u & 0x7FFFFFFFU; }  

    uint mixBits(uint u, uint v) { return hiBit(u)|loBits(v); } 

    uint state[N+1];     
    uint *next;          
    int left;   

    public:

    MTRnd() : left(-1) {}  

    void SeedMT(uint seed)
    {
        uint x = (seed | 1U) & 0xFFFFFFFFU, *s = state;
        int j;
        for(left=0, *s++=x, j=N; --j; *s++ = (x*=69069U) & 0xFFFFFFFFU);
    }

    uint ReloadMT()
    {
        uint *p0=state, *p2=state+2, *pM=state+M, s0, s1;
        int j;
        if(left < -1) SeedMT(4357U);
        left=N-1, next=state+1;
        for(s0=state[0], s1=state[1], j=N-M+1; --j; s0=s1, s1=*p2++) *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
        for(pM=state, j=M; --j; s0=s1, s1=*p2++) *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
        s1=state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
        s1 ^= (s1 >> 11);
        s1 ^= (s1 <<  7) & 0x9D2C5680U;
        s1 ^= (s1 << 15) & 0xEFC60000U;
        return(s1 ^ (s1 >> 18));
    }

    uint RandomMT()
    {
        uint y;
        if(--left < 0) return(ReloadMT());
        y  = *next++;
        y ^= (y >> 11);
        y ^= (y <<  7) & 0x9D2C5680U;
        y ^= (y << 15) & 0xEFC60000U;
        return(y ^ (y >> 18));
    }

    int operator()(int max) { return RandomMT()%max; }
};


// for use with vc++ crtdbg
/*
#ifdef _DEBUG
void *__cdecl operator new(size_t n, const char *fn, int l) { return ::operator new(n, 1, fn, l); }
void __cdecl operator delete(void *p, const char *fn, int l) { ::operator delete(p, 1, fn, l); }
#define new new(__FILE__,__LINE__)
#endif 
*/

