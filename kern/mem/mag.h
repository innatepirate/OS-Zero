#ifndef __MEM_MAG_H__
#define __MEM_MAG_H__

#include <zero/param.h>
#include <zero/mtx.h>

#include <kern/task.h>
#include <kern/mem/slab.h>

#define MAGMIN     (1UL << MAGMINLOG2)
#define MAGMINLOG2 PAGESIZELOG2

#define maglkq(tab, bkt)   mtxlk(&(tab)[bkt], MEMPID)
#define magunlkq(tab, bkt) mtxunlk(&(tab)[bkt], MEMPID)

#define magpop(mp)         ((mp)->ptab[((mp)->ndx)++])
#define magpush(mp, ptr)   ((mp)->ptab[--((mp)->ndx)] = (ptr))
#define magfull(mp)        ((mp)->ndx == (mp)->n)
#define magempty(mp)       (!(mp)->ndx)
struct maghdr {
#if (MAGLK)
    volatile long  lk;
#endif
#if (MAGBITMAP)
    uintptr_t      base;
#endif
    volatile long  n;
    volatile long  ndx;
    volatile long  bkt;
    struct maghdr *prev;
    struct maghdr *next;
    void          *ptab[1UL << (SLABMINLOG2 - MAGMINLOG2)];
#if (MAGBITMAP)
    uint8_t        bmap[1UL << (SLABMINLOG2 - MAGMINLOG2 - 3)];
#endif
};

#define magnum(ptr, base)                                               \
    (((uintptr_t)(ptr) - base) >> SLABMINLOG2)
#define magslabadr(ptr)                                                 \
    ((void *)((uintptr_t)(ptr) & ~(SLABMIN - 1)))
#define maggethdr(ptr, tab, base)                                       \
    (!(ptr) ? NULL : (tab) + magnum(ptr, base))

#endif /* __MEM_MAG_H__ */

