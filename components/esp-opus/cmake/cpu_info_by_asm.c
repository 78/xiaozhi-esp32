#include <stdio.h>
int main() {
    unsigned int CPUInfo0;
    unsigned int CPUInfo1;
    unsigned int CPUInfo2;
    unsigned int CPUInfo3;
    unsigned int InfoType;
#if defined(__i386__) && defined(__PIC__)
/* %ebx is PIC register in 32-bit, so mustn't clobber it. */
    __asm__ __volatile__ (
        "xchg %%ebx, %1\n"
        "cpuid\n"
        "xchg %%ebx, %1\n":
        "=a" (CPUInfo0),
        "=r" (CPUInfo1),
        "=c" (CPUInfo2),
        "=d" (CPUInfo3) :
        "0" (InfoType), "2" (0)
    );
#else
    __asm__ __volatile__ (
        "cpuid":
        "=a" (CPUInfo0),
        "=b" (CPUInfo1),
        "=c" (CPUInfo2),
        "=d" (CPUInfo3) :
        "0" (InfoType), "2" (0)
    );
#endif
    return 0;
}
