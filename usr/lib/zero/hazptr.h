#ifndef __ZERO_HAZPTR_H__
#define __ZERO_HAZPTR_H__

#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include <zero/param.h>
#include <zero/mtx.h>

#if !defined(HAZPTR_T)
#define HAZPTR_T         void *
#define HAZPTR_NONE      NULL
#endif
#define HAZPTR_BIN_ITEMS (PAGESIZE / sizeof(void *))

#define HAZPTR_BIN_INIT     (1L << 0)
struct hazptrbin {
    volatile long   flg;                // bin flag-bits
    long            cur;                // top of stack
    long            lim;                // maximum stack index (# of items)
    HAZPTR_T       *tab;                // hazard pointers
    void          (*free)(void *);      // reclaim function
    void          (*buf)();             // buffer function
    uint8_t         _pad[CLSIZE         // pad to cacheline boundary
                         - 3 * sizeof(long)
                         - 3 * sizeof(void *)];
};

/* push hazard pointer into bin; wait till success */
static __inline__ void
hazptrpush(struct hazptrbin *bin, HAZPTR_T ptr)
{
    long ndx;
    
    do {
        ndx = m_fetchadd(&bin->cur, -1);
        if (ndx > 0) {
            ndx--;
            bin->tab[ndx] = ptr;

            return;
        } else {
            m_atominc(&bin->cur);
            m_waitint();
        }
    } while (1);

    /* NOTREACHED */
    return;
}

/* pop hazard pointer from bin; wait till success */
static __inline__ HAZPTR_T
hazptrpop(struct hazptrbin *bin)
{
    long     lim = bin->lim;
    long     ndx;
    HAZPTR_T ptr;

    do {
        ndx = m_fetchadd(&bin->cur, 1);
        if (ndx < lim) {
            ptr = bin->tab[ndx];

            return ptr;
        } else {
            m_atomdec(&bin->cur);
            m_waitint();
        }
    } while (1);

    /* NOTREACHED */
    return ptr;
}

/* pop hazard pointer from bin; return HAZPTR_NONE if none present */
static __inline__ HAZPTR_T
hazptrget(struct hazptrbin *bin)
{
    HAZPTR_T ptr = HAZPTR_NONE;
    long     ndx = m_fetchadd(&bin->cur, 1);
    long     lim = bin->lim;

    if (ndx < lim) {
        ptr = bin->tab[ndx];
    } else {
        m_atomdec(&bin->cur);
    }

    /* NOTREACHED */
    return ptr;
}

#endif /* __ZERO_HAZPTR_H__ */

