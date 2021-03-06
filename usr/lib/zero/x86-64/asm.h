#ifndef __ZERO_X86_64_ASM_H__
#define __ZERO_X86_64_ASM_H__

#include <stddef.h>
#include <stdint.h>
#include <zero/cdefs.h>
#include <zero/types.h>
#include <zero/x86/asm.h>

extern uint64_t asmgetpc(void);

typedef volatile int64_t m_atomic_t;

#define m_atominc(p)               m_atominc64(p)
#define m_atomdec(p)               m_atomdec64(p)
#define m_atomswap(p, val)         m_xchg64(p, val)
#define m_fetchadd(p, val)         m_xadd64(p, val)
#define m_fetchaddu(p, val)        m_xaddu64(p, val)
#define m_cmpswap(p, want, val)    m_cmpxchg64(p, want, val)
#define m_cmpswapu(p, want, val)   m_cmpxchgu64(p, want, val)
#define m_cmpswapptr(p, want, val) m_cmpxchg64ptr(p, want, val)
#define m_cmpswapdbl(p, want, val) m_cmpxchg128(p, want, val)
#define m_setbit(p, ndx)           m_setbit64(p, ndx)
#define m_clrbit(p, ndx)           m_clrbit64(p, ndx)
#define m_flipbit(p, ndx)          m_flipbit64(p, ndx)
#define m_cmpsetbit(p, ndx)        m_cmpsetbit64(p, ndx)
#define m_cmpclrbit(p, ndx)        m_cmpclrbit64(p, ndx)
#define m_scanlo1bit(l)            m_bsf64(l)
#define m_scanhi1bit(l)            m_bsr64(l)

#define __RIPFRAMEOFS              offsetof(struct m_stkframe, pc)

static INLINE void
m_getretadr(void **pp)
{
    void *_ptr;
    
    __asm__ __volatile__ ("movq %c1(%%rbp), %0\n"
                          : "=r" (_ptr)
                          : "i" (__RIPFRAMEOFS));
    *pp = _ptr;

    return;
}

static INLINE void
m_getfrmadr(void **pp)
{
    void *_ptr;

    __asm__ __volatile__ ("movq %%rbp, %0\n" : "=r" (_ptr));
    *pp = _ptr;

    return;
}

static INLINE void
m_getfrmadr2(void *fp, void **pp)
{
    void *_ptr;

    __asm__ __volatile__ ("movq %1, %%rax\n"
                          "movq (%%rax), %0\n"
                          : "=r" (_ptr)
                          : "rm" (fp)
                          : "rax");
    *pp = _ptr;

    return;
}

static INLINE void
m_loadretadr(void *frm, void **pp)
{
    void *_ptr;

    __asm__ __volatile__ ("movq %c1(%2), %0\n"
                          : "=r" (_ptr)
                          : "i" (__RIPFRAMEOFS), "r" (frm));
    *pp = _ptr;

    return;
}

static INLINE void
m_getretfrmadr(void **pp)
{
    void *_ptr;

    __asm__ __volatile__ ("movq (%%rbp), %0\n" : "=r" (_ptr));
    *pp = _ptr;

    return;
}

/* atomic increment operation */
static __inline__ void
m_atominc64(m_atomic64_t *p)
{
    __asm__ __volatile__ ("lock incq %0\n"
                          : "+m" (*(p))
                          :
                          : "memory");

    return;
}

/* atomic decrement operation */
static __inline__ void
m_atomdec64(m_atomic64_t *p)
{
    __asm__ __volatile__ ("lock decq %0\n"
                          : "+m" (*(p))
                          :
                          : "memory");

    return;
}

static __inline__ long
m_xchg64(m_atomic64_t *p,
         long val)
{
    __asm__ __volatile__ ("lock xchgq %%rax, %q1\n"
                          : "=a" (val)
                          : "m" (*(p))
                          : "cc", "memory");
    
    return val;
}

/*
 * atomic fetch and add
 * - let *p = *p + val
 * - return original *p
 */
static __inline__ long
m_xadd64(m_atomic64_t *p,
         long val)
{
    __asm__ __volatile__ ("lock xaddq %1, %q0\n"
                          : "+m" (*(p)), "=a" (val)
                          :
                          : "cc", "memory");

    return val;
}

/*
 * atomic fetch and add
 * - let *p = *p + val
 * - return original *p
 */
static __inline__ uint64_t
m_xaddu64(volatile uint64_t *p,
          uint64_t val)
{
    __asm__ __volatile__ ("lock xaddq %1, %q0\n"
                          : "+m" (*(p)), "=a" (val)
                          :
                          : "cc", "memory");

    return val;
}

/*
 * atomic compare and exchange longword
 * - if *p == want, let *p = val
 * - return nonzero on success, zero on failure
 */
static __inline__ long
m_cmpxchg64(m_atomic64_t *p,
           long want,
           long val)
{
    long res;
    
    __asm__ __volatile__ ("lock cmpxchgq %1, %2\n"
                          : "=a" (res)
                          : "q" (val), "m" (*(p)), "0" (want)
                          : "memory");
    
    return (res == want);
}

/*
 * atomic compare and exchange unsigned longword
 * - if *p == want, let *p = val
 * - return nonzero on success, zero on failure
 */
static __inline__ uint64_t
m_cmpxchgu64(volatile uint64_t *p,
             uint64_t want,
             uint64_t val)
{
    uint64_t res;
    
    __asm__ __volatile__ ("lock cmpxchgq %1, %2\n"
                          : "=a" (res)
                          : "q" (val), "m" (*(p)), "0" (want)
                          : "memory");
    
    return (res == want);
}

/*
 * atomic compare and exchange pointer
 * - if *p == want, let *p = val
 * - return nonzero on success, zero on failure
 */
static __inline__ long
m_cmpxchg64ptr(m_atomic64_t *p,
               long *want,
               void *val)
{
    long *res;
    
    __asm__ __volatile__("lock cmpxchgq %1, %2\n"
                         : "=a" (res)
                         : "q" (val), "m" (*(p)), "0" (want)
                         : "memory");
    
    return (res == want);
}

#if defined(__GNUC__) && 0

/*
 * atomic 128-bit compare and swap
 * - if *p == want, let *p = val
 * - return original nonzero on success, zero on failure
 */
static __inline__ long
m_cmpxchg128(m_atomic64_t *p64,
             long *want,
             long *val)
{
    return __sync_bool_compare_and_swap(p64, want, val);
}

#elif defined(_MSC_VER)

static __inline__ long
m_cmpxchg128(m_atomic64_t long *p64,
             long long *want,
             long long *val)
{
    long long     lo = val[0];
    long long     hi = val[1];
    unsigned char res;

    res = InterlockedCompareExchange128(p64, hi, lo, want);

    return res;
}

#else

/*
 * atomic 128-bit compare and swap
 * - if *p == want, let *p = val
 * - return original nonzero on success, zero on failure
 */
static __inline__ long
m_cmpxchg128(m_atomic64_t *p64,
             long *want,
             long *val)
{
    uint64_t rax = want[0];
    uint64_t rdx = want[1];
    uint64_t rbx = val[0];
    uint64_t rcx = val[1];
    long     res = 0;
    
    __asm__ __volatile__ ("lock cmpxchg16b %1\n"
                          "setz %b0\n"
                          : "=q" (res), "+m" (*p64), "+a" (rax), "+d" (rdx)
                          : "b" (rbx), "c" (rcx)
                          : "cc");

    return res;
}

#endif

/* atomic set bit operation */
static INLINE void
m_setbit64(m_atomic64_t *p, long ndx)
{
    __asm__ __volatile__ ("lock btsq %1, %0\n"
                          : "=m" (*(p))
                          : "Ir" (ndx)
                          : "memory");

    return;
}

/* atomic reset/clear bit operation */
static INLINE void
m_clrbit64(m_atomic64_t *p, long ndx)
{
    __asm__ __volatile__ ("lock btrq %1, %0\n"
                          : "=m" (*(p))
                          : "Ir" (ndx));

    return;
}

/* atomic flip/toggle bit operation */
static INLINE void
m_flipbit64(m_atomic64_t *p, long ndx)
{
    __asm__ __volatile__ ("lock btcq %1, %0\n"
                          : "=m" (*(p))
                          : "Ir" (ndx));

    return;
}

/* atomic set and test bit operation; returns the old value */
static __inline__ long
m_cmpsetbit64(m_atomic64_t *p, long ndx)
{
    long val;

    if (IMMEDIATE(ndx)) {
        __asm__ __volatile__ ("xorq %1, %1\n"
                              "lock btsq %2, %0\n"
                              "jnc 1f\n"
                              "incq %1\n"
                              "1:\n"
                              : "+m" (*(p)), "=r" (val)
                              : "i" (ndx)
                              : "cc", "memory");
    } else {
        __asm__ __volatile__ ("xorq %1, %1\n"
                              "lock btsq %2, %0\n"
                              "jnc 1f\n"
                              "incq %1\n"
                              "1:\n"
                              : "+m" (*(p)), "=r" (val)
                              : "r" (ndx)
                              : "cc", "memory");
    }

    return val;
}

/* atomic clear bit operation */
static __inline__ long
m_cmpclrbit64(m_atomic64_t *p, long ndx)
{
    long val;

    if (IMMEDIATE(ndx)) {
        __asm__ __volatile__ ("xorq %1, %1\n"
                              "lock btrq %2, %0\n"
                              "jnc 1f\n"
                              "incq %1\n"
                              "1:\n"
                              : "+m" (*(p)), "=r" (val)
                              : "i" (ndx)
                              : "cc", "memory");
    } else {
        __asm__ __volatile__ ("xorq %1, %1\n"
                              "lock btrq %2, %0\n"
                              "jnc 1f\n"
                              "incq %1\n"
                              "1:\n"
                              : "+m" (*(p)), "=r" (val)
                              : "r" (ndx)
                              : "cc", "memory");
    }

    return val;
}

static __inline__ long
m_bsf64(uint64_t val)
{
    long ret = ~0UL;

    __asm__ __volatile__ ("bsfq %1, %0\n" : "=r" (ret) : "rm" (val));

    return ret;
}

static __inline__ long
m_bsr64(uint64_t val)
{
    long ret = ~0UL;

    __asm__ __volatile__ ("bsrq %1, %0\n" : "=r" (ret) : "rm" (val));

    return ret;
}

#endif /* __ZERO_X86_64_ASM_H__ */

