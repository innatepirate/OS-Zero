#include <stddef.h>
#include <stdio.h>
#if (MEMDEBUG)
#include <stdio.h>
//#include <assert.h>
//#include <crash.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <zero/cdefs.h>
#include <zero/param.h>
#include <zero/unix.h>
#include <zero/spin.h>
#include <zero/mem.h>
#include <zero/hash.h>

static pthread_once_t               g_initonce = PTHREAD_ONCE_INIT;
static pthread_key_t                g_thrkey;
THREADLOCAL zerospin                g_memtlsinitlk = ZEROSPININITVAL;
THREADLOCAL MEMUWORD_T              g_memtlsinit;
THREADLOCAL volatile struct memtls *g_memtls;
struct mem                          g_mem;
#if (MEMSTAT)
struct memstat                      g_memstat;
#endif

static void
memfreetls(void *arg)
{
    void                   *adr = arg;
    volatile struct membkt *src;
    volatile struct membkt *dest;
    MEMUWORD_T              slot;
    MEMUWORD_T              n;
    struct membuf          *head;
    struct membuf          *buf;
    MEMADR_T                upval;

    if (g_memtls) {
        for (slot = 0 ; slot < PTRBITS ; slot++) {
            src = &g_memtls->smallbin[slot];
            dest = &g_mem.smallbin[slot];
            head = src->list;
            if (head) {
                buf = head;
                buf->bkt = dest;
                while (buf->next) {
                    buf = buf->next;
                    buf->bkt = dest;
                }
                n = src->nbuf;
#if (MEMDEBUGDEADLOCK)
                memlkbitln(dest);
#else
                memlkbit(&dest->list);
#endif
                upval = (MEMADR_T)dest->list;
                upval &= ~MEMLKBIT;
                dest->nbuf += n;
                buf->prev = NULL;
                buf->next = (struct membuf *)upval;
                if (upval) {
                    ((struct membuf *)upval)->prev = buf;
                }
#if (MEMDEBUGDEADLOCK)
                dest->line = __LINE__;
#endif
                m_syncwrite((m_atomic_t *)&dest->list, (m_atomic_t)head);
            }
        }
        for (slot = 0 ; slot < MEMPAGESLOTS ; slot++) {
            src = &g_memtls->pagebin[slot];
            dest = &g_mem.pagebin[slot];
            head = src->list;
            if (head) {
                buf = head;
                buf->bkt = dest;
                while (buf->next) {
                    buf = buf->next;
                    buf->bkt = dest;
                }
                n = src->nbuf;
#if (MEMDEBUGDEADLOCK)
                memlkbitln(dest);
#else
                memlkbit(&dest->list);
#endif
                upval = (MEMADR_T)dest->list;
                upval &= ~MEMLKBIT;
                dest->nbuf += n;
                buf->prev = NULL;
                buf->next = (struct membuf *)upval;
                if (upval) {
                    ((struct membuf *)upval)->prev = buf;
                }
#if (MEMDEBUGDEADLOCK)
                dest->line = __LINE__;
#endif
                m_syncwrite((m_atomic_t *)&dest->list, (m_atomic_t)head);
            }
        }
        unmapanon(adr, memtlssize());
    }

    return;
}

#if (MEM_LK_TYPE == MEM_LK_PRIO)
static unsigned long
memgetprioval(void)
{
    unsigned long val;

//    fmtxlk(&g_mem.priolk);
    val = g_mem.prioval;
    val++;
    val &= sizeof(long) * CHAR_BIT - 1;
    g_mem.prioval = val;
//    fmtxunlk(&g_mem.priolk);

    return val;
}
#endif

struct memtls *
meminittls(void)
{
    struct memtls *tls = NULL;
    struct memtls *adr = NULL;
#if (MEM_LK_TYPE == MEM_LK_PRIO)
    unsigned long  val;
#endif

    pthread_once(&g_initonce, meminit);
    tls = mapanon(0, memtlssize());
    if (tls != MAP_FAILED) {
//        adr = (struct memtls *)memgentlsadr((MEMPTR_T)adr);
        adr = tls;
#if 0
        tls = (struct memtls *)memgenptrcl(adr, memtlssize(),
                                           sizeof(struct memtls));
#endif
#if (MEM_LK_TYPE == MEM_LK_PRIO)
        val = memgetprioval();
        priolkinit(&adr->priolkdata, val);
#endif
        pthread_setspecific(g_thrkey, tls);
        g_memtls = adr;
        g_memtlsinit = 1;
    }

    return adr;
}

static void
memprefork(void)
{
    MEMUWORD_T slot;

//    fmtxlk(&g_mem.priolk);
    memgetlk(&g_mem.initlk);
    memgetlk(&g_mem.heaplk);
    for (slot = 0 ; slot < PTRBITS ; slot++) {
#if (MEMDEBUGDEADLOCK)
        memlkbitln(&g_mem.smallbin[slot]);
        memlkbitln(&g_mem.bigbin[slot]);
#else
        memlkbit(&g_mem.smallbin[slot].list);
        memlkbit(&g_mem.bigbin[slot].list);
#endif
    }
    for (slot = 0 ; slot < MEMPAGESLOTS ; slot++) {
#if (MEMDEBUGDEADLOCK)
        memlkbitln(&g_mem.pagebin[slot]);
#else
        memlkbit(&g_mem.pagebin[slot].list);
#endif
    }
#if (MEMMULTITAB)
    for (slot = 0 ; slot < MEMLVL1ITEMS ; slot++) {
        memlkbit(&g_mem.tab[slot].tab);
    }
#elif (!MEMLFHASH)
    for (slot = 0 ; slot < MEMHASHITEMS ; slot++) {
        memlkbit(&g_mem.hash[slot].chain);
    }
#endif

    return;
}

static void
mempostfork(void)
{
    MEMUWORD_T slot;

#if (MEMMULTITAB)
    for (slot = 0 ; slot < MEMLVL1ITEMS ; slot++) {
        memrelbit(&g_mem.tab[slot].tab);
    }
#elif (!MEMLFHASH)
    for (slot = 0 ; slot < MEMHASHITEMS ; slot++) {
        memrelbit(&g_mem.hash[slot].chain);
    }
#endif
    for (slot = 0 ; slot < MEMPAGESLOTS ; slot++) {
#if (MEMDEBUGDEADLOCK)
        memrelbitln(&g_mem.pagebin[slot]);
#else
        memrelbit(&g_mem.pagebin[slot].list);
#endif
    }
    for (slot = 0 ; slot < PTRBITS ; slot++) {
#if (MEMDEBUGDEADLOCK)
        memrelbitln(&g_mem.bigbin[slot]);
        memrelbitln(&g_mem.smallbin[slot]);
#else
        memrelbit(&g_mem.bigbin[slot].list);
        memrelbit(&g_mem.smallbin[slot].list);
#endif
    }
    memrellk(&g_mem.heaplk);
    memrellk(&g_mem.initlk);
//    fmtxunlk(&g_mem.priolk);

    return;
}

void
memexit(int sig)
{
#if (MEMSTAT)
    memprintstat();
#endif

    exit(sig);
}

void
memquit(int sig)
{
#if (MEMSTAT)
    memprintstat();
#endif
    fprintf(stderr, "CAUGHT signal %d, aborting\n", sig);
    
    abort();
}

void
meminit(void)
{
    void       *heap;
    long        ofs;
    void       *ptr;
    MEMPTR_T   *adr;
    MEMUWORD_T  slot;

    memgetlk(&g_mem.initlk);
    signal(SIGQUIT, memexit);
    signal(SIGINT, memexit);
#if 0
    signal(SIGSEGV, memquit);
    signal(SIGABRT, memquit);
    signal(SIGTERM, memquit);
#endif
    pthread_atfork(memprefork, mempostfork, mempostfork);
    pthread_key_create(&g_thrkey, memfreetls);
#if (MEMSTAT)
    atexit(memprintstat);
#endif
    adr = mapanon(0, 3 * (2 * PTRBITS + MEMPAGESLOTS) * sizeof(MEMUWORD_T));
    if (adr == MAP_FAILED) {

        crash(adr != MAP_FAILED);
    }
    g_mem.bufvals.nblk[MEMSMALLBUF] = (MEMUWORD_T *)&adr[0];
    g_mem.bufvals.nblk[MEMPAGEBUF] = (MEMUWORD_T *)&adr[PTRBITS];
    g_mem.bufvals.nblk[MEMBIGBUF] = (MEMUWORD_T *)&adr[PTRBITS + MEMPAGESLOTS];
    g_mem.bufvals.ntls[MEMSMALLBUF] = (MEMUWORD_T *)&adr[2 * PTRBITS + MEMPAGESLOTS];
    g_mem.bufvals.ntls[MEMPAGEBUF] = (MEMUWORD_T *)&adr[3 * PTRBITS + MEMPAGESLOTS];
    g_mem.bufvals.ntls[MEMBIGBUF] = (MEMUWORD_T *)&adr[3 * PTRBITS + 2 * MEMPAGESLOTS];
    g_mem.bufvals.nglob[MEMSMALLBUF] = (MEMUWORD_T *)&adr[4 * PTRBITS + 2 * MEMPAGESLOTS];
    g_mem.bufvals.nglob[MEMPAGEBUF] = (MEMUWORD_T *)&adr[5 * PTRBITS + 2 * MEMPAGESLOTS];
    g_mem.bufvals.nglob[MEMBIGBUF] = (MEMUWORD_T *)&adr[5 * PTRBITS + 3 * MEMPAGESLOTS];
    for (slot = 0 ; slot < PTRBITS ; slot++) {
        g_mem.bufvals.nblk[MEMSMALLBUF][slot] = memnbufblk(MEMSMALLBUF, slot);
        g_mem.bufvals.nblk[MEMBIGBUF][slot] = memnbufblk(MEMSMALLBUF, slot);
        g_mem.bufvals.ntls[MEMSMALLBUF][slot] = memnbuftls(MEMSMALLBUF, slot);
//        g_mem.bufvals.ntls[MEMBIGBUF][slot] = memnbufblk(MEMSMALLBUF, slot);
        g_mem.bufvals.nglob[MEMSMALLBUF][slot] = memnbufglob(MEMSMALLBUF, slot);
        g_mem.bufvals.nglob[MEMBIGBUF][slot] = memnbufglob(MEMSMALLBUF, slot);
    }
    for (slot = 0 ; slot < MEMPAGESLOTS ; slot++) {
        g_mem.bufvals.nblk[MEMPAGEBUF][slot] = memnbufblk(MEMPAGEBUF, slot);
        g_mem.bufvals.ntls[MEMPAGEBUF][slot] = memnbuftls(MEMPAGEBUF, slot);
        g_mem.bufvals.nglob[MEMPAGEBUF][slot] = memnbufglob(MEMPAGEBUF, slot);
    }
#if (MEMMULTITAB)
    ptr = mapanon(0, MEMLVL1ITEMS * sizeof(struct memtab));
#elif (MEMARRAYHASH) || (MEMNEWHASH)
    ptr = mapanon(0, MEMHASHITEMS * sizeof(struct memhashlist));
#elif (MEMHASH)
    ptr = mapanon(0, MEMHASHITEMS * sizeof(struct memhash));
#if (MEMSTAT)
    g_memstat.nbhash += MEMHASHITEMS * sizeof(struct memhash);
#endif
#elif (MEMHUGELOCK)
    ptr = mapanon(0, MEMLVL1ITEMS * sizeof(struct memtabl0));
#else
    ptr = mapanon(0, MEMLVL1ITEMS * sizeof(struct memtab));
#endif
    if (ptr == MAP_FAILED) {

        crash(ptr != MAP_FAILED);
    }
#if (MEMMULTITAB)
    g_mem.tab = ptr;
#elif (MEMHASH) || (MEMARRAYHASH) || (MEMNEWHASH)
    g_mem.hash = ptr;
#endif
//    memgetlk(&g_mem.heaplk);
    heap = growheap(0);
    ofs = (1UL << PAGESIZELOG2) - ((long)heap & (PAGESIZE - 1));
    if (ofs != PAGESIZE) {
        growheap(ofs);
    }
//    memrellk(&g_mem.heaplk);
    memrellk(&g_mem.initlk);

    return;
}

MEMPTR_T
memputptr(struct membuf *buf, MEMPTR_T ptr, MEMUWORD_T size, MEMUWORD_T align,
          MEMUWORD_T id)
{
    MEMUWORD_T type = memgetbuftype(buf);
//    MEMUWORD_T bsz = membufblksize(buf, type, slot);
//    MEMUWORD_T id;
    MEMPTR_T   adr = ptr;
//    MEMADR_T   delta;

    if ((MEMADR_T)ptr & (align - 1)) {
        ptr = memalignptr(adr, align);
#if 0
        delta = (MEMADR_T)ptr - adr;
        bsz -= delta;
        if (bsz < size) {
            ptr = memgenptr(ptr, bsz, size);
        }
#endif
    }
    if (type != MEMPAGEBUF) {
        membufsetptr(buf, ptr, adr);
    } else {
//        id = membufpageid(buf, ptr);
        membufsetpageadr(buf, id, adr);
    }

    return ptr;
}

static struct membuf *
memallocsmallbuf(MEMUWORD_T slot)
{
    MEMPTR_T       adr = SBRK_FAILED;
    MEMWORD_T      bufsz = memsmallbufsize(slot);
    MEMUWORD_T     nblk = MEMBUFBLKS;
//    MEMUWORD_T     type = MEMSMALLBUF;
    MEMUWORD_T     flg = 0;
    struct membuf *buf;

    if (!(g_mem.flg & MEMNOHEAPBIT)) {
        /* try to allocate from heap (sbrk()) */
        memgetlk(&g_mem.heaplk);
        adr = growheap(bufsz);
        flg = MEMHEAPBIT;
        if (adr != SBRK_FAILED) {
#if (MEMSTAT)
            g_memstat.nbheap += bufsz;
#endif
            g_mem.flg |= MEMHEAPBIT;
        } else {
            g_mem.flg |= MEMNOHEAPBIT;
            memrellk(&g_mem.heaplk);
        }
    }
    if (adr == SBRK_FAILED) {
        /* sbrk() failed or was skipped, let's try mmap() */
        adr = mapanon(0, bufsz);
        if (adr == MAP_FAILED) {
#if defined(ENOMEM)
            errno = ENOMEM;
#endif

            return NULL;
        }
#if (MEMSTAT)
        g_memstat.nbsmall += bufsz;
        g_memstat.nbmap += bufsz;
#endif
    }
    buf = (struct membuf *)adr;
#if (MEMBITFIELD)
    memsetbufflg(buf, 1);
#else
    buf->info = flg;    // possible MEMHEAPBIT
#endif
    memsetbufslot(buf, slot);
    memsetbufnblk(buf, nblk);
    memsetbuftype(buf, MEMSMALLBUF);
    buf->size = bufsz;
//    buf->ptrtab = (MEMPTR_T *)((MEMPTR_T)buf + membufhdrsize());
#if (MEMSTAT)
    g_memstat.nbsmall += bufsz;
    g_memstat.nbheap += bufsz;
    g_memstat.nbbook += membufblkofs();
#endif
#if (MEMTEST)
    _memchkbuf(buf, slot, MEMSMALLBUF, nblk, flg, __FUNCTION__);
#endif

    return buf;
}

static void *
meminitsmallbuf(struct membuf *buf, MEMUWORD_T size, MEMUWORD_T align)
{
    MEMUWORD_T nblk = MEMBUFBLKS;
    MEMPTR_T   adr = (MEMPTR_T)buf;
    MEMPTR_T   ptr = adr + membufblkofs();

    /* initialise freemap */
    membufinitfree(buf, nblk);
    buf->base = ptr;
    nblk--;
    VALGRINDMKPOOL(ptr, 0, 0);
    memsetbufnfree(buf, nblk);
    ptr = memputptr(buf, ptr, size, align, 0);
    memsetbuf(ptr, buf, 0);
#if (MEMTEST)
    _memchkptr(buf, ptr);
#endif

    return ptr;
}

static struct membuf *
memallocpagebuf(MEMUWORD_T slot, MEMUWORD_T nblk)
{
    MEMUWORD_T     mapsz = mempagebufsize(slot, nblk);
//    MEMUWORD_T     type = MEMPAGEBUF;
    MEMPTR_T       adr;
    struct membuf *buf;

    /* mmap() blocks */
    adr = mapanon(0, mapsz);
    buf = (struct membuf *)adr;
#if (MEMBITFIELD)
    memclrbufflg(buf, 1);
#else
    buf->info = 0;
#endif
    memsetbufslot(buf, slot);
    memsetbufnblk(buf, nblk);
    memsetbuftype(buf, MEMPAGEBUF);
    buf->size = mapsz;
    if (adr == MAP_FAILED) {
        
        return NULL;
    }
#if (MEMSTAT)
    g_memstat.nbpage += mapsz;
    g_memstat.nbmap += mapsz;
    g_memstat.nbbook += membufblkofs();
#endif
//    buf->ptrtab = (MEMPTR_T *)((MEMPTR_T)buf + membufhdrsize());
#if (MEMTEST)
    _memchkbuf(buf, slot, MEMPAGEBUF, nblk, 0, __FUNCTION__);
#endif

    return buf;
}

static void *
meminitpagebuf(struct membuf *buf,
               MEMUWORD_T size, MEMUWORD_T align,
               MEMUWORD_T nblk)
{
    MEMPTR_T adr = (MEMPTR_T)buf;
    MEMPTR_T ptr = adr + membufblkofs();

    /* initialise freemap */
    membufinitfree(buf, nblk);
    buf->base = ptr;
    nblk--;
    VALGRINDMKPOOL(ptr, 0, 0);
    memsetbufnfree(buf, nblk);
    ptr = memputptr(buf, ptr, size, align, 0);
    memsetbuf(ptr, buf, 0);
#if (MEMTEST)
    _memchkptr(buf, ptr);
#endif

    return ptr;
}

static struct membuf *
memallocbigbuf(MEMUWORD_T slot, MEMUWORD_T nblk)
{
    MEMUWORD_T     mapsz = membigbufsize(slot, nblk);
//    MEMUWORD_T     type = MEMBIGBUF;
    MEMPTR_T       adr;
    struct membuf *buf;

    /* mmap() blocks */
    adr = mapanon(0, mapsz);
    if (adr == MAP_FAILED) {
        
        return NULL;
    }
    buf = (struct membuf *)adr;
#if (MEMBITFIELD)
    memclrbufflg(buf, 1);
#else
    buf->info = 0;
#endif
    memsetbufslot(buf, slot);
    memsetbufnblk(buf, nblk);
    memsetbuftype(buf, MEMBIGBUF);
#if (MEMSTAT)
    g_memstat.nbbig += mapsz;
    g_memstat.nbmap += mapsz;
    g_memstat.nbbook += membufblkofs();
#endif
    buf->size = mapsz;
//    buf->ptrtab = (MEMPTR_T *)((MEMPTR_T)buf + membufhdrsize());
#if (MEMTEST)
    _memchkbuf(buf, slot, MEMBIGBUF, nblk, 0, __FUNCTION__);
#endif

    return buf;
}

static void *
meminitbigbuf(struct membuf *buf,
              MEMUWORD_T size, MEMUWORD_T align,
              MEMUWORD_T nblk)
{
    MEMPTR_T adr = (MEMPTR_T)buf;
    MEMPTR_T ptr = adr + membufblkofs();

    membufinitfree(buf, nblk);
    buf->base = ptr;
    nblk--;
    VALGRINDMKPOOL(ptr, 0, 0);
    memsetbufnfree(buf, nblk);
    ptr = memputptr(buf, ptr, size, align, 0);
    memsetbuf(ptr, buf, 0);
#if (MEMTEST)
    _memchkptr(buf, ptr);
#endif

    return ptr;
}

void *
memgetbufblktls(struct membuf *head, volatile struct membkt *bkt,
                MEMUWORD_T size, MEMUWORD_T align)
{
    void          *ptr = NULL;
    MEMUWORD_T     nfree = memgetbufnfree(head);
    MEMUWORD_T     type = memgetbuftype(head);
    MEMUWORD_T     id = membufgetfree(head);

#if (MEMDEBUG)
    crash(nfree != 0);
#endif
    nfree--;
    if (type != MEMPAGEBUF) {
        ptr = membufblkadr(head, id);
    } else {
        ptr = membufpageadr(head, id);
    }
    memsetbufnfree(head, nfree);
    ptr = memputptr(head, ptr, size, align, id);
    memsetbuf(ptr, head, id);
#if (MEMTEST)
    _memchkptr(head, ptr);
#endif
    VALGRINDPOOLALLOC(head->base, ptr, size);
    if (!nfree) {
        if (head->next) {
            head->next->prev = NULL;
        }
        bkt->nbuf--;
#if (MEMDEBUGDEADLOCK)
        bkt->line = __LINE__;
#endif
        bkt->list = head->next;
        head->bkt = NULL;
        head->prev = NULL;
        head->next = NULL;
    }

    return ptr;
}

void *
memgetbufblkglob(struct membuf *head, volatile struct membkt *bkt,
                 MEMUWORD_T size, MEMUWORD_T align)
{
    void          *ptr = NULL;
    MEMUWORD_T     nfree = memgetbufnfree(head);
    MEMUWORD_T     type = memgetbuftype(head);
    MEMUWORD_T     id = membufgetfree(head);

#if (MEMDEBUG)
    crash(nfree != 0);
#endif
    nfree--;
    if (type != MEMPAGEBUF) {
        ptr = membufblkadr(head, id);
    } else {
        ptr = membufpageadr(head, id);
    }
    memsetbufnfree(head, nfree);
    ptr = memputptr(head, ptr, size, align, id);
    memsetbuf(ptr, head, id);
#if (MEMTEST)
    _memchkptr(head, ptr);
#endif
    VALGRINDPOOLALLOC(head->base, ptr, size);
    if (!nfree) {
        if (head->next) {
            head->next->prev = NULL;
        }
        bkt->nbuf--;
#if (MEMDEBUGDEADLOCK)
        bkt->line = __LINE__;
#endif
        m_syncwrite((m_atomic_t *)&bkt->list, (m_atomic_t)head->next);
        head->bkt = NULL;
        head->prev = NULL;
        head->next = NULL;
    } else {
#if (MEMDEBUGDEADLOCK)
        memrelbitln(bkt);
#else
        memrelbit(&bkt->list);
#endif
    }

    return ptr;
}

#if (MEMMULTITAB)

MEMPTR_T
memsetbuf(MEMPTR_T ptr, struct membuf *buf)
{
    MEMADR_T        val = (MEMADR_T)buf;
    struct memtab  *itab;
    struct memtab  *tab;
    struct memitem *item;
    long             k1;
    long             k2;
    long             k3;
    long             k4;
    void            *pstk[2] = { NULL };
    
    memgetkeybits(ptr, k1, k2, k3, k4);
//    memgetlk(&g_mem.tab[k1].lk);
    itab = g_mem.tab[k1].tab;
    if (!itab) {
        itab = mapanon(0, MEMLVLITEMS * sizeof(struct memtab));
        if (itab == MAP_FAILED) {
//            memrellk(&g_mem.tab[k1].lk);
            
            return NULL;
        }
        pstk[0] = itab;
        g_mem.tab[k1].tab = itab;
    }
    tab = &itab[k2];
    itab = tab->tab;
    if (!itab) {
        itab = mapanon(0, MEMLVLITEMS * sizeof(struct memtab));
        if (itab == MAP_FAILED) {
            unmapanon(pstk[0], MEMLVLITEMS * sizeof(struct memtab));
//            memrellk(&g_mem.tab[k1].lk);

            return NULL;
        }
        pstk[1] = itab;
        tab->tab = itab;
    }
    tab = &itab[k3];
    itab = tab->tab;
    if (!itab) {
        itab = mapanon(0, MEMLVLITEMS * sizeof(struct memitem));
        if (itab == MAP_FAILED) {
            unmapanon(pstk[0], MEMLVLITEMS * sizeof(struct memtab));
            unmapanon(pstk[1], MEMLVLITEMS * sizeof(struct memtab));
//            memrellk(&g_mem.tab[k1].lk);
            
            return NULL;
        }
        tab->tab = itab;
    }
    item = (struct memitem *)&itab[k4];
//    item->nref++;
    item->val = val;
//    memrellk(&g_mem.tab[k1].lk);
    
    return ptr;
}

struct membuf *
memfindbuf(void *ptr, long rel)
{
    struct membuf  *buf = NULL;
    struct memtab  *itab;
    struct memtab  *tab;
    struct memitem *item;
    long            k1;
    long            k2;
    long            k3;
    long            k4;

    memgetkeybits(ptr, k1, k2, k3, k4);
//    memgetlk(&g_mem.tab[k1].lk);
    itab = g_mem.tab[k1].tab;
    if (itab) {
        tab = &itab[k2];
        itab = tab->tab;
        if (itab) {
            tab = &itab[k3];
            itab = tab->tab;
            if (itab) {
                item = (struct memitem *)&itab[k4];
                buf = item->val;
                if (rel) {
                    memputblk(ptr, buf);
#if 0
                    if (!--item->nref) {
                        item->val = 0;
                    }
#endif
                }
            }
        }
    }
//    memrellk(&g_mem.tab[k1].lk);

    return buf;
}

#elif (MEMNEWHASH)

static void
meminithashitem(MEMPTR_T data)
{
    struct memhash *item = (struct memhash *)data;
    MEMUWORD_T     *uptr;

    data += offsetof(struct memhash, data);
    item->chain = NULL;
    uptr = (MEMUWORD_T *)data;
    uptr = memgenhashtabadr(uptr);
    item->ntab = 0;
    item->tab = (struct memhashitem *)uptr;
    item->list = NULL;

    return;
}

static struct memhash *
memgethashitem(void)
{
    struct memhash *item = NULL;
    MEMPTR_T        first;
    struct memhash *prev;
    struct memhash *cur;
    MEMPTR_T        next;
    MEMUWORD_T      bsz;
    MEMADR_T        upval;
    long            n;

    memlkbit(&g_mem.hashbuf);
    upval = (MEMADR_T)g_mem.hashbuf;
    upval &= ~MEMLKBIT;
    if (upval) {
        item = (struct memhash *)upval;
        m_syncwrite((m_atomic_t *)&g_mem.hashbuf, (m_atomic_t)item->chain);
//        meminithashitem(item);
    } else {
        n = 4 * PAGESIZE / memhashsize();
        bsz = 4 * PAGESIZE;
#if (MEMSTAT)
        g_memstat.nbhash += bsz;
#endif
        item = mapanon(0, bsz);
        first = (MEMPTR_T)item;
        if (item == MAP_FAILED) {

            crash(item != MAP_FAILED);
        }
        meminithashitem(first);
        upval = (MEMADR_T)g_mem.hashbuf;
        first += memhashsize();
        upval &= ~MEMLKBIT;
        next = first;
        while (--n) {
            prev = (struct memhash *)next;
            meminithashitem(next);
            next += memhashsize();
            prev->chain = (struct memhash *)next;
        }
        cur = (struct memhash *)prev;
        cur->chain = (struct memhash *)upval;
        m_syncwrite((m_atomic_t *)&g_mem.hashbuf, (m_atomic_t)first);
    }

    return item;
}

static void
membufhashitem(struct memhash *item)
{
    MEMADR_T upval;

    memlkbit(&g_mem.hashbuf);
    upval = (MEMADR_T)g_mem.hashbuf;
    upval &= ~MEMLKBIT;
    item->chain = (struct memhash *)upval;
    m_syncwrite((m_atomic_t *)&g_mem.hashbuf, (m_atomic_t)item);

    return;
}

MEMADR_T
membufop(MEMPTR_T ptr, MEMWORD_T op, struct membuf *buf, MEMUWORD_T id)
{
    MEMADR_T            adr = (MEMADR_T)ptr;
    MEMADR_T            page = adr >> PAGESIZELOG2;
    MEMUWORD_T          key = memhashptr(page) & (MEMHASHITEMS - 1);
    MEMADR_T            desc;
    MEMADR_T            val;
    MEMADR_T            bufval;
    MEMADR_T            upval;
    struct memhash     *blk;
    struct memhash     *prev;
    struct memhashitem *slot;
    struct memhashitem *src;
    MEMUWORD_T          lim;
    MEMUWORD_T          n;
    MEMUWORD_T          found;

//    fprintf(stderr, "locking hash chain %lx\n", key);
#if (!MEMLFHASH)
    memlkbit(&g_mem.hash[key].chain);
#endif
    upval = (MEMADR_T)g_mem.hash[key].chain;
#if (!MEMLFHASH)
    upval &= ~MEMLKBIT;
#endif
    found = 0;
    blk = (struct memhash *)upval;
    while ((blk) && !found) {
        lim = blk->ntab;
        src = blk->tab;
        prev = NULL;
        do {
            n = min(lim, 4);
            switch (n) {
                case 4:
                    slot = &src[3];
                    if (slot->page == page) {
                        found++;
                        
                        break;
                    }
                case 3:
                    slot = &src[2];
                    if (slot->page == page) {
                        found++;
                        
                        break;
                    }
                case 2:
                    slot = &src[1];
                    if (slot->page == page) {
                        found++;
                        
                        break;
                    }
                case 1:
                    slot = &src[0];
                    if (slot->page == page) {
                        found++;
                        
                        break;
                    }
                case 0:
                default:
                    
                    break;
            }
            lim -= n;
            src += n;
        } while ((lim) && !found);
        if (!found) {
            prev = blk;
            blk = blk->chain;
        }
    }
//    upval = (MEMADR_T)g_mem.hash[key].chain;
    if (!found) {
        if (op == MEMHASHDEL || op == MEMHASHCHK) {
#if (!MEMLFHASH)
            memrelbit(&g_mem.hash[key].chain);
#endif

            return MEMHASHNOTFOUND;
        } else {
            desc = (MEMADR_T)buf;
//            upval &= ~MEMLKBIT;
            blk = (struct memhash *)upval;
            desc |= id;
            if (blk) {
                prev = NULL;
                do {
                    n = blk->ntab;
                    if (n < MEMHASHITEMS) {
                        slot = &blk->tab[n];
                        found++;
                        n++;
                        slot->nref = 1;
                        slot->nact = 1;
                        slot->page = page;
                        blk->ntab = n;
                        slot->val = desc;
#if (MEMDEBUG)
                        crash(slot != NULL);
#endif
                        slot->val = desc;
                        if (prev) {
                            prev->chain = blk->chain;
                            blk->chain = (struct memhash *)upval;
#if (MEMHASHLOCK)
                            g_mem.hash[key].chain = blk;
#elif (MEMNEWHASH)
#if (!MEMLFHASH)
                            memrelbit(&g_mem.hash[key].chain);
#endif
#else
                            m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                                        (m_atomic_t)blk);
#endif
                        } else {
#if (!MEMLFHASH)
                            memrelbit(&g_mem.hash[key].chain);
#endif
                        }

                        return desc;
                    }
                    if (!found) {
                        prev = blk;
                        blk = blk->chain;
                    }
                } while (!found && (blk));
            }
            if (!found) {
                blk = memgethashitem();
                slot = blk->tab;
                blk->ntab = 1;
                slot->nref = 1;
                slot->nact = 1;
                blk->chain = (struct memhash *)upval;
                blk->list = &g_mem.hash[key];
                slot->page = page;
                slot->val = desc;
#if (MEMHASHLOCK)
                g_mem.hash[key].chain = blk;
                memrellk(&g_mem.hash[key].lk);
#else
                m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                            (m_atomic_t)blk);
#endif
            } else {
#if (!MEMLFHASH)
                memrelbit(&g_mem.hash[key].chain);
#endif
            }
#if (MEMDEBUG)
            crash(desc != 0);
#endif
            
            return desc;
        }
    }
    desc = slot->val;
//    upval &= ~MEMLKBIT;
    bufval = desc;
    slot->nact++;
//    desc &= ~MEMPAGEINFOMASK;
    if (op == MEMHASHDEL) {
        bufval &= ~MEMPAGEINFOMASK;
        slot->nref--;
        id = desc & MEMPAGEINFOMASK;
        n = blk->ntab;
        memputblk(ptr, (struct membuf *)bufval, id);
        if (!slot->nref) {
            if (n == 1) {
                if (prev) {
                    prev->chain = blk->chain;
#if (!MEMLFHASH)
                    memrelbit(&g_mem.hash[key].chain);
#endif
                } else {
                    m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                                (m_atomic_t)blk->chain);
                }
                membufhashitem(blk);
            } else {
                src = &blk->tab[n];
                slot->nref = src->nref;
                slot->nact = src->nact;
                n--;
//                upval &= ~MEMLKBIT;
                slot->page = src->page;
                slot->val = src->val;
                blk->ntab = n;
            }
        }
#if (MEMDEBUG)
        crash(desc != 0);
#endif
#if (!MEMLFHASH)
        memrelbit(&g_mem.hash[key].chain);
#endif
        
        return bufval;
    } else if (op == MEMHASHADD) {
#if 0
        if (op == MEMHASHCHK) {
            desc = MEMHASHFOUND;
        }
#endif
        slot->nref += op;
    }
//    upval &= ~MEMLKBIT;
    if (prev) {
        prev->chain = blk->chain;
        blk->chain = (struct memhash *)upval;
        m_syncwrite((m_atomic_t *)&g_mem.hash[key].chain,
                    (m_atomic_t)blk);
#if (!MEMLFHASH)
    } else {
        memrelbit(&g_mem.hash[key].chain);
#endif
    }
#if (MEMDEBUG)
    crash(desc != 0);
#endif
    
    return desc;
}

MEMPTR_T
memsetbuf(MEMPTR_T ptr, struct membuf *buf, MEMUWORD_T id)
{
    MEMADR_T desc = membufop(ptr, MEMHASHADD, buf, id);

    return (MEMPTR_T)desc;
}

#endif

static MEMADR_T
memopenbuf(volatile struct membkt *bkt)
{
    MEMADR_T upval;

    if (!bkt->list) {

        return 0;
    }
#if (MEMDEBUGDEADLOCK)
    memlkbitln(bkt);
#else
    memlkbit(&bkt->list);
#endif
    upval = (MEMADR_T)bkt->list;
    upval &= ~MEMLKBIT;
    if (!upval) {
#if (MEMDEBUGDEADLOCK)
        memrelbitln(bkt);
#else
        memrelbit(&bkt->list);
#endif
    }

    return upval;
}

MEMPTR_T
memtryblk(MEMUWORD_T slot, MEMUWORD_T type,
          MEMUWORD_T size, MEMUWORD_T align,
          volatile struct membkt *bkt1, volatile struct membkt *bkt2)
{
//    MEMADR_T                upval = (bkt1) ? bkt1->list : 0;
    MEMADR_T                upval = (bkt1) ? (MEMADR_T)bkt1->list : 0;
    struct membuf          *buf = (struct membuf *)upval;
    MEMPTR_T                ptr = NULL;
    MEMUWORD_T              flg = 0;
    MEMUWORD_T              nblk = memgetnbufblk(slot, type);
    volatile struct membkt *dest;
    
    if (buf) {
        ptr = memgetbufblktls(buf, bkt1, size, align);
#if (MEMTEST)
//        fprintf(stderr, "GET: %p (%p)\n", buf, ptr);
//        memprintbuf(buf, NULL);
#endif
    } else {
        if (bkt2) {
            upval = memopenbuf(bkt2);
            buf = (struct membuf *)upval;
        }
        if (buf) {
            ptr = memgetbufblkglob(buf, bkt2, size, align);
#if (MEMTEST)
//            fprintf(stderr, "GET: %p (%p)\n", buf, ptr);
//            memprintbuf(buf, NULL);
#endif
        } else {
            if (type == MEMSMALLBUF) {
                buf = memallocsmallbuf(slot);
                if (buf) {
                    ptr = meminitsmallbuf(buf, size, align);
                }
            } else if (type == MEMPAGEBUF) {
                buf = memallocpagebuf(slot, nblk);
                if (buf) {
                    ptr = meminitpagebuf(buf, size, align, nblk);
                }
            } else {
                buf = memallocbigbuf(slot, nblk);
                if (buf) {
                    ptr = meminitbigbuf(buf, size, align, nblk);
                }
            }
            if (type == MEMSMALLBUF && (ptr)) {
                flg = buf->flg;
                if (flg & MEMHEAPBIT) {                    
                    memlkbit(&g_mem.heap);
                    upval = (MEMADR_T)g_mem.heap;
                    upval &= ~MEMLKBIT;
                    buf->heap = (struct membuf *)upval;
                    /* this unlocks the global heap (low-bit becomes zero) */
                    m_syncwrite((m_atomic_t *)&g_mem.heap, (m_atomic_t)buf);
                    memrellk(&g_mem.heaplk);
                }
            }
            if (nblk > 1) {
                if (type == MEMSMALLBUF) {
                    dest = &g_memtls->smallbin[slot];
                } else if (type == MEMPAGEBUF) {
                    dest = &g_memtls->pagebin[slot];
                } else {
                    dest = &g_mem.bigbin[slot];
                }
#if (MEMTEST)
//                fprintf(stderr, "ADD: %p (%p)\n", buf, dest);
//                memprintbuf(buf, NULL);
#endif
#if (MEMDEBUGDEADLOCK)
//                fprintf(stderr, "OWNED: %ld\n", dest->line);
                memlkbitln(dest);
#else
                memlkbit(&dest->list);
#endif
                upval = (MEMADR_T)dest->list;
                upval &= ~MEMLKBIT;
                buf->prev = NULL;
                buf->next = (struct membuf *)upval;
                if (upval) {
                    ((struct membuf *)upval)->prev = buf;
                }
                buf->bkt = dest;
#if (MEMDEBUGDEADLOCK)
                dest->line = __LINE__;
#endif
                dest->nbuf++;
                m_syncwrite((m_atomic_t *)&dest->list, (m_atomic_t)buf);
            }
        }
    }
    
    return ptr;
}

MEMPTR_T
memgetblk(MEMUWORD_T slot, MEMUWORD_T type, MEMUWORD_T size, MEMUWORD_T align)
{
    MEMPTR_T       ptr = NULL;
    
    if (type == MEMSMALLBUF) {
        ptr = memtryblk(slot, MEMSMALLBUF,
                        size, align,
                        &g_memtls->smallbin[slot], &g_mem.smallbin[slot]);
#if (MEMDEBUG)
        crash(ptr != NULL);
#endif
    } else if (type == MEMPAGEBUF) {
        ptr = memtryblk(slot, MEMPAGEBUF,
                        size, align,
                        &g_memtls->pagebin[slot], &g_mem.pagebin[slot]);
#if (MEMDEBUG)
        crash(ptr != NULL);
#endif
    } else {
        ptr = memtryblk(slot, MEMBIGBUF,
                        size, align,
                        NULL, &g_mem.bigbin[slot]);
#if (MEMDEBUG)
        crash(ptr != NULL);
#endif
    }
#if (MEMDEBUG)
    crash(ptr != NULL);
#endif

    return ptr;
}

void
memdequeuebuftls(struct membuf *buf, volatile struct membkt *src)
{
    struct membuf *head = src->list;
    
    if ((buf->prev) && (buf->next)) {
        buf->prev->next = buf->next;
        buf->next->prev = buf->prev;
    } else if (buf->prev) {
        buf->prev->next = NULL;
    } else if (buf->next) {
        buf->next->prev = NULL;
        head = buf->next;
    } else {
        head = NULL;
    }
#if 0
    m_syncwrite((m_atomic_t)&src->list, (m_atomic_t)head);
#endif
    src->list = head;

    return;
}

void
memdequeuebufglob(struct membuf *buf, volatile struct membkt *src)
{
    struct membuf *head;
    MEMADR_T       upval;

    upval = (MEMADR_T)src->list;
    upval &= ~MEMLKBIT;
    if ((buf->prev) && (buf->next)) {
        buf->prev->next = buf->next;
        buf->next->prev = buf->prev;
        head = (struct membuf *)upval;
    } else if (buf->prev) {
        buf->prev->next = NULL;
        head = (struct membuf *)upval;
    } else if (buf->next) {
        buf->next->prev = NULL;
        head = buf->next;
    } else {
        head = NULL;
    }
#if (MEMDEBUGDEADLOCK)
    src->line = __LINE__;
#endif
    m_syncwrite((m_atomic_t)&src->list, (m_atomic_t)head);

    return;
}

void
memrelbuf(MEMUWORD_T slot, MEMUWORD_T type,
          struct membuf *buf, volatile struct membkt *src)
{
    struct membkt *dest;
    MEMADR_T       upval;
    
    if (type == MEMSMALLBUF) {
        dest = &g_mem.smallbin[slot];
        if (src == &g_memtls->smallbin[slot]) {
            if (src->nbuf < memgetnbuftls(slot, MEMSMALLBUF)) {

                return;
            } else {
                memdequeuebuftls(buf, src);
#if (MEMDEBUGDEADLOCK)
                memlkbitln(dest);
#else
                memlkbit(&dest->list);
#endif
                upval = (MEMADR_T)dest->list;
                upval &= ~MEMLKBIT;
                buf->prev = NULL;
                buf->next = (struct membuf *)upval;
                if (upval) {
                    ((struct membuf *)upval)->prev = buf;
                }
                dest->nbuf++;
#if (MEMDEBUGDEADLOCK)
                dest->line = __LINE__;
#endif
                m_syncwrite((m_atomic_t *)&dest->list, (m_atomic_t *)buf);
            }

            return;
        }
    } else if (type == MEMPAGEBUF) {
        dest = &g_mem.pagebin[slot];
        if (src == &g_memtls->pagebin[slot]) {
            if (src->nbuf < memgetnbuftls(slot, MEMPAGEBUF)) {

                return;
            } else {
                memdequeuebuftls(buf, src);
                if (dest->nbuf < memgetnbufglob(slot, MEMPAGEBUF)) {
#if (MEMDEBUGDEADLOCK)
                    memlkbitln(dest);
#else
                    memlkbit(&dest->list);
#endif
                    upval = (MEMADR_T)dest->list;
                    upval &= ~MEMLKBIT;
                    buf->prev = NULL;
                    buf->next = (struct membuf *)upval;
                    if (upval) {
                        ((struct membuf *)upval)->prev = buf;
                    }
                    dest->nbuf++;
#if (MEMDEBUGDEADLOCK)
                    dest->line = __LINE__;
#endif
                    m_syncwrite((m_atomic_t *)&dest->list, (m_atomic_t *)buf);
                } else {
                    VALGRINDRMPOOL(buf->base);
#if (MEMSTAT)
                    g_memstat.nbmap -= buf->size;
                    g_memstat.nbbig -= buf->size;
                    g_memstat.nbbook -= membufblkofs();
#endif
                    unmapanon(buf, buf->size);
                }

                return;
            }
        } else if (src->nbuf >= memgetnbufglob(slot, MEMPAGEBUF)) {
            memdequeuebufglob(buf, src);
            VALGRINDRMPOOL(buf->base);
#if (MEMSTAT)
            g_memstat.nbmap -= buf->size;
            g_memstat.nbpage -= buf->size;
            g_memstat.nbbook -= membufblkofs();
#endif
            unmapanon(buf, buf->size);
        }
    } else if (type == MEMBIGBUF) {
        if (src->nbuf >= memgetnbufglob(slot, MEMBIGBUF)) {
            memdequeuebufglob(buf, src);
            VALGRINDRMPOOL(buf->base);
#if (MEMSTAT)
            g_memstat.nbmap -= buf->size;
            g_memstat.nbbig -= buf->size;
            g_memstat.nbbook -= membufblkofs();
#endif
            unmapanon(buf, buf->size);
        }
    }

    return;
}

void
memputblk(void *ptr, struct membuf *buf, MEMUWORD_T id)
{
    volatile struct membkt *bkt = buf->bkt;
    volatile struct membkt *dest;
    MEMUWORD_T              type = memgetbuftype(buf);
    MEMUWORD_T              slot = memgetbufslot(buf);
    MEMUWORD_T              nblk;
    MEMUWORD_T              nfree;
//    MEMUWORD_T     id;
    MEMADR_T                upval;
    MEMUWORD_T              lock = 0;

#if (MEMTEST)
    _memchkptr(buf, ptr);
#endif
    if (!bkt) {
        if (type == MEMSMALLBUF) {
            dest = &g_mem.smallbin[slot];
        } else if (type == MEMPAGEBUF) {
            dest = &g_mem.pagebin[slot];
        } else if (type == MEMBIGBUF) {
            dest = &g_mem.bigbin[slot];
        }
    } else if (bkt != &g_memtls->smallbin[slot]
               && bkt != &g_memtls->pagebin[slot]) {
#if (MEMDEBUGDEADLOCK)
        memlkbitln(bkt);
#else
        memlkbit(&bkt->list);
#endif
        lock = 1;
    }
    nblk = memgetbufnblk(buf);
    nfree = memgetbufnfree(buf);
#if (MEMDEBUG)
    crash(nfree < nblk);
#endif
    nfree++;
    if (type != MEMPAGEBUF) {
        id = membufblkid(buf, ptr);
        membufsetptr(buf, ptr, NULL);
    } else {
//        id = membufpageid(buf, ptr);
        membufsetpageadr(buf, id, NULL);
    }
    setbit(buf->freemap, id);
    memsetbufnfree(buf, nfree);
    VALGRINDPOOLFREE(buf->base, ptr);
    if (nfree == nblk) {
        memrelbuf(slot, type, buf, bkt);
        if ((lock) && bkt != dest) {
#if (MEMDEBUGDEADLOCK)
            memrelbitln(bkt);
#else
            memrelbit(&bkt->list);
#endif
        }
    } else if (nfree == 1) {
#if (MEMDEBUGDEADLOCK)
        memlkbitln(dest);
#else
        memlkbit(&dest->list);
#endif
        upval = (MEMADR_T)dest->list;
        upval &= ~MEMLKBIT;
        buf->prev = NULL;
        buf->next = (struct membuf *)upval;
        if (upval) {
            ((struct membuf *)upval)->prev = buf;
        }
        buf->bkt = dest;
        dest->nbuf++;
#if (MEMDEBUGDEADLOCK)
        dest->line = __LINE__;
#endif
        m_syncwrite((m_atomic_t *)&dest->list, (m_atomic_t)buf);
    } else if (lock) {
#if (MEMDEBUGDEADLOCK)
        memrelbitln(bkt);
#else
        memrelbit(&bkt->list);
#endif
    }
    
    return;
}

