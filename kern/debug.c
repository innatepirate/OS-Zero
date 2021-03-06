#include <stdint.h>
#include <zero/types.h>
#include <kern/util.h>
#include <kern/debug.h>

void
m_printregs(void)
{
    int32_t eax;
    int32_t ecx;
    int32_t edx;
    int32_t ebx;
    int32_t esp;
    int32_t ebp;
    int32_t esi;
    int32_t edi;

    __asm__ __volatile__ ("movl %%eax, %0\n"
                          "movl %%ecx, %1\n"
                          "movl %%edx, %2\n"
                          "movl %%ebx, %3\n"
                          "movl %%esp, %4\n"
                          "movl %%ebp, %5\n"
                          "movl %%esi, %6\n"
                          "movl %%edi, %7\n"
                          : "=m" (eax), "=m" (ecx), "=m" (edx), "=m" (ebx),
                            "=m" (esp), "=m" (ebp), "=m" (esi), "=m" (edi));
    kprintf("general-purpose registers\n");
    kprintf("-------------------------\n");
    kprintf("eax\t0x%lx\n", eax);
    kprintf("ecx\t0x%lx\n", ecx);
    kprintf("edx\t0x%lx\n", edx);
    kprintf("ebx\t0x%lx\n", ebx);
    kprintf("esp\t0x%lx\n", esp);
    kprintf("ebp\t0x%lx\n", ebp);
    kprintf("esi\t0x%lx\n", esi);
    kprintf("edi\t0x%lx\n", edi);

    return;
}

void
m_printgenregs(struct m_genregs *genregs)
{
    kprintf("general-purpose registers\n");
    kprintf("-------------------------\n");
    kprintf("eax\t0x%lx\n", genregs-> eax);
    kprintf("ecx\t0x%lx\n", genregs-> ecx);
    kprintf("edx\t0x%lx\n", genregs-> edx);
    kprintf("ebx\t0x%lx\n", genregs-> ebx);
    kprintf("esp\t0x%lx\n", genregs-> esp);
    kprintf("ebp\t0x%lx\n", genregs-> ebp);
    kprintf("esi\t0x%lx\n", genregs-> esi);
    kprintf("edi\t0x%lx\n", genregs-> edi);

    return;
}

void
m_printsegregs(struct m_segregs *segregs)
{
    kprintf("segment registers\n");
    kprintf("-----------------\n");
    kprintf("ds\t0x%lx\n", segregs->ds);
    kprintf("es\t0x%lx\n", segregs->es);
    kprintf("fs\t0x%lx\n", segregs->fs);
    kprintf("gs\t0x%lx\n", segregs->gs);
}

void
m_printtrapframe(struct m_trapframe *trapframe, long havestk)
{
    kprintf("jump/stack frame\n");
    kprintf("----------------\n");
    kprintf("eip\t0x%lx\n", trapframe->eip);
    kprintf("cs\t%hx\n", trapframe->cs);
    kprintf("eflags\t0x%lx\n", trapframe->eflags);
    if (havestk) {
        kprintf("uesp\t0x%lx\n", trapframe->uesp);
        kprintf("uss\t%hx\n", trapframe->uss);
    }

    return;
}

void
m_printtask(struct m_task *task, long flg)
{
    kprintf("flg\t%lx\n", task->flg);
    kprintf("pdbr\t%lx\n", task->tcb.pdbr);
    m_printgenregs(&task->tcb.genregs);
    m_printsegregs(&task->tcb.segregs);
    m_printtrapframe(&task->tcb.trapframe, flg & M_TRAPFRAMESTK);

    return;
}

