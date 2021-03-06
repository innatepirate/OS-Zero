#ifndef __ZERO_X86_ASM_H__
#define __ZERO_X86_ASM_H__

//#define frameisusr(tcb) ((tcb)->frame.cs == UTEXTSEL)

#include <stdint.h>

typedef volatile int8_t  m_atomic8_t;
typedef volatile int16_t m_atomic16_t;
typedef volatile int32_t m_atomic32_t;
typedef volatile int64_t m_atomic64_t;

/* memory barrier */
#define m_membar()     __asm__ __volatile__ ("mfence\n" : : : "memory")
/* memory read barrier */
#define m_memrdbar()   __asm__ __volatile__ ("lfence\n" : : : "memory")
/* memory write barrier */
#define m_memwrbar()   __asm__ __volatile__ ("sfence\n" : : : "memory")
/* wait for an interrupt */
#define m_waitint()    __asm__ __volatile__ ("pause\n"  : : : "memory")
/* atomic fetch and add, 16-bit version */
#define m_fetchadd16(p, val)     m_xadd16(p, val)
#define m_fetchaddu16(p, val)    m_xaddu16(p, val)
/* atomic fetch and add, 32-bit version */
#define m_fetchadd32(p, val)     m_xadd32(p, val)
#define m_fetchaddu32(p, val)    m_xaddu32(p, val)
/* atomic compare and swap byte */
#define m_cmpswapb(p, want, val) m_cmpxchg8(p, want, val)

/* atomic increment operation */
static __inline__ void
m_atominc32(m_atomic32_t *p)
{
    __asm__ __volatile__ ("lock incl %0\n"
                          : "+m" (*(p))
                          :
                          : "memory");

    return;
}

/* atomic decrement operation */
static __inline__ void
m_atomdec32(m_atomic32_t *p)
{
    __asm__ __volatile__ ("lock decl %0\n"
                          : "+m" (*(p))
                          :
                          : "memory");

    return;
}

/* atomic exchange operation */
static __inline__ int
m_xchg32(m_atomic32_t *p,
         int val)
{
    int res;

    __asm__ __volatile__ ("lock xchgl %0, %2\n"
                          : "+m" (*p), "=a" (res)
                          : "r" (val)
                          : "cc", "memory");

    return res;
}

/*
 * atomic fetch and add
 * - let *p = *p + val
 * - return original *p
 */
static __inline__ short
m_xadd16(volatile short *p,
         short val)
{
    __asm__ __volatile__ ("lock xaddw %1, %w0\n"
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
static __inline__ unsigned short
m_xaddu16(volatile unsigned short *p,
          unsigned short val)
{
    __asm__ __volatile__ ("lock xaddw %1, %w0\n"
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
static __inline__ int
m_xadd32(m_atomic32_t *p,
         int val)
{
    __asm__ __volatile__ ("lock xaddl %1, %0\n"
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
static __inline__ unsigned int
m_xaddu32(volatile unsigned int *p,
          unsigned int val)
{
    __asm__ __volatile__ ("lock xaddl %1, %0\n"
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
static __inline__ int
m_cmpxchg32(m_atomic32_t *p,
            int want,
            int val)
{
    int res;
    
    __asm__ __volatile__("lock cmpxchgl %1, %2\n"
                         : "=a" (res)
                         : "q" (val), "m" (*(p)), "0" (want)
                         : "memory");
    
    return (res == want);
}

/* atomic set bit operation */
static INLINE void
m_setbit32(m_atomic32_t *p, int ndx)
{
    __asm__ __volatile__ ("lock btsl %1, %0\n"
                          : "=m" (*(p))
                          : "Ir" (ndx)
                          : "memory");

    return;
}

/* atomic reset/clear bit operation */
static INLINE void
m_clrbit32(m_atomic32_t *p, int ndx)
{
    __asm__ __volatile__ ("lock btrl %1, %0\n"
                          : "=m" (*((uint8_t *)(p) + (ndx >> 3)))
                          : "Ir" (ndx));

    return;
}

/* atomic flip/toggle bit operation */
static INLINE void
m_flipbit32(m_atomic32_t *p, int ndx)
{
    __asm__ __volatile__ ("lock btcl %1, %0\n"
                          : "=m" (*((uint8_t *)(p) + (ndx >> 3)))
                          : "Ir" (ndx));

    return;
}

/* atomic set and test bit operation; returns the old value */
static __inline__ int
m_cmpsetbit32(m_atomic32_t *p, int ndx)
{
    int val;

    if (IMMEDIATE(ndx)) {
        __asm__ __volatile__ ("xorl %1, %1\n"
                              "lock btsl %2, %0\n"
                              "jnc 1f\n"
                              "incl %1\n"
                              "1:\n"
                              : "+m" (*(p)), "=r" (val)
                              : "i" (ndx)
                              : "cc", "memory");
    } else {
        __asm__ __volatile__ ("xorl %1, %1\n"
                              "lock btsl %2, %0\n"
                              "jnc 1f\n"
                              "incl %1\n"
                              "1:\n"
                              : "+m" (*(p)), "=r" (val)
                              : "r" (ndx)
                              : "cc", "memory");
    }

    return val;
}

/* atomic clear bit operation */
static __inline__ int
m_cmpclrbit32(m_atomic32_t *p, int ndx)
{
    int val;

    if (IMMEDIATE(ndx)) {
        __asm__ __volatile__ ("xorl %1, %1\n"
                              "lock btrl %2, %0\n"
                              "jnc 1f\n"
                              "incl %1\n"
                              "1:\n"
                              : "+m" (*(p)), "=r" (val)
                              : "i" (ndx)
                              : "cc", "memory");
    } else {
        __asm__ __volatile__ ("xorl %1, %1\n"
                              "lock btrl %2, %0\n"
                              "jnc 1f\n"
                              "incl %1\n"
                              "1:\n"
                              : "+m" (*(p)), "=r" (val)
                              : "r" (ndx)
                              : "cc", "memory");
    }

    return val;
}

#if defined(__GNUC__)
#define m_atomor(p, val)  __sync_or_and_fetch((p), (val))
#define m_atomand(p, val) __sync_and_and_fetch((p), (val))
#endif

/*
 * atomic compare and exchange byte
 * - if *p == want, let *p = val
 * - return original *p
 */
static __inline__ char
m_cmpxchg8(volatile char *p,
           char want,
           char val)
{
    long res;

    __asm__ __volatile__ ("lock cmpxchgb %b1, %2\n"
                          : "=a" (res)
                          : "q" (val), "m" (*(p)), "0" (want)
                          : "memory");

    return (res == want);
}

static __inline__ int
m_bsf32(unsigned int val)
{
    int ret = ~0;

    __asm__ __volatile__ ("bsfl %1, %0\n" : "=r" (ret) : "rm" (val));

    return ret;
}

static __inline__ int
m_bsr32(unsigned int val)
{
    int ret = ~0;

    __asm__ __volatile__ ("bsrl %1, %0\n" : "=r" (ret) : "rm" (val));

    return ret;
}

#endif /* __ZERO_X86_ASM_H__ */

