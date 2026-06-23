#include <windows.h>

#include <hal/aosl_hal_atomic.h>

intptr_t aosl_hal_atomic_read(const intptr_t *v)
{
#if defined(_WIN64)
    return (intptr_t)InterlockedCompareExchange64((volatile LONG64 *)v, 0, 0);
#else
    return (intptr_t)InterlockedCompareExchange((volatile LONG *)v, 0, 0);
#endif
}

void aosl_hal_atomic_set(intptr_t *v, intptr_t i)
{
    (void)aosl_hal_atomic_xchg(v, i);
}

intptr_t aosl_hal_atomic_inc(intptr_t *v)
{
#if defined(_WIN64)
    return (intptr_t)InterlockedExchangeAdd64((volatile LONG64 *)v, 1);
#else
    return (intptr_t)InterlockedExchangeAdd((volatile LONG *)v, 1);
#endif
}

intptr_t aosl_hal_atomic_dec(intptr_t *v)
{
#if defined(_WIN64)
    return (intptr_t)InterlockedExchangeAdd64((volatile LONG64 *)v, -1);
#else
    return (intptr_t)InterlockedExchangeAdd((volatile LONG *)v, -1);
#endif
}

intptr_t aosl_hal_atomic_add(intptr_t i, intptr_t *v)
{
#if defined(_WIN64)
    return (intptr_t)(InterlockedExchangeAdd64((volatile LONG64 *)v, (LONG64)i) + i);
#else
    return (intptr_t)(InterlockedExchangeAdd((volatile LONG *)v, (LONG)i) + i);
#endif
}

intptr_t aosl_hal_atomic_sub(intptr_t i, intptr_t *v)
{
    return aosl_hal_atomic_add(-i, v);
}

intptr_t aosl_hal_atomic_cmpxchg(intptr_t *v, intptr_t old, intptr_t new)
{
    return (intptr_t)InterlockedCompareExchangePointer((PVOID volatile *)v, (PVOID)new, (PVOID)old);
}

intptr_t aosl_hal_atomic_xchg(intptr_t *v, intptr_t new)
{
    return (intptr_t)InterlockedExchangePointer((PVOID volatile *)v, (PVOID)new);
}

void aosl_hal_mb(void)
{
    MemoryBarrier();
}

void aosl_hal_rmb(void)
{
    MemoryBarrier();
}

void aosl_hal_wmb(void)
{
    MemoryBarrier();
}
