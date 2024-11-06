#include <cpuid.h>
int main() {
    unsigned int CPUInfo0;
    unsigned int CPUInfo1;
    unsigned int CPUInfo2;
    unsigned int CPUInfo3;
    unsigned int InfoType;
    return __get_cpuid_count(InfoType, 0, &CPUInfo0, &CPUInfo1, &CPUInfo2, &CPUInfo3);
}
