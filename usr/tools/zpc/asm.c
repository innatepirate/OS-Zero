#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <zas/zas.h>
#include <zpc/asm.h>

char *zpcopnametab[ZPCNASMOP]
= {
    "ILL",
    "not",
    "and",
    "or",
    "xor",
    "shr",
    "shra",
    "shl",
    "ror",
    "rol",
    "inc",
    "dec",
    "add",
    "sub",
    "mul",
    "div",
    "mod",
    "bz",
    "bnz",
    "blt",
    "ble",
    "bgt",
    "bge",
    "mov",
    "movd",
    "movb",
    "movw",
    "jmp",
    "call",
    "ret",
    "trap",
    "iret",
    "thr"
};
uint64_t zpcintregs[ZPCNREG];

zasuword_t
zpcgetreg(uint8_t *str, uint8_t **retptr)
{
    zasuword_t reg = 0;
    
#if (ASMDEBUG)
    fprintf(stderr, "getreg: %s\n", str);
#endif
    if (*str == 'r') {
        str++;
        while ((*str) && isdigit(*str)) {
            reg *= 10;
            reg += *str - '0';
            str++;
        }
        while (*str == ')' || *str == ',') {
            str++;
        }
        *retptr = str;
    } else if (*str == 'v') {
        str++;
        while ((*str) && isdigit(*str)) {
            reg *= 10;
            reg += *str - '0';
            str++;
        }
        while (*str == ')' || *str == ',') {
            str++;
        }
        reg |= ZPCREGVECBIT;
        *retptr = str;
    } else if (*str == 's' && str[1] == 't') {
        str += 2;
        while ((*str) && isdigit(*str)) {
            reg *= 10;
            reg += *str - '0';
            str++;
        }
        while (*str == ')' || *str == ',') {
            str++;
        }
        reg |= ZPCREGSTKBIT;
        *retptr = str;
    } else {
        fprintf(stderr, "invalid register name %s\n", str);
        
        exit(1);
    }
    
    return reg;
}

