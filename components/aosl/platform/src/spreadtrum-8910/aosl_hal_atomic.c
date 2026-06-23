#include <stdbool.h>

#include <hal/aosl_hal_atomic.h>

intptr_t aosl_hal_atomic_read(const intptr_t *v)
{
  return __atomic_load_n((volatile intptr_t *)v, __ATOMIC_SEQ_CST);
}

void aosl_hal_atomic_set(intptr_t *v, intptr_t i)
{
  __atomic_store_n((volatile intptr_t *)v, i, __ATOMIC_SEQ_CST);
}

intptr_t aosl_hal_atomic_inc(intptr_t *v)
{
  return __atomic_fetch_add((volatile intptr_t *)v, 1, __ATOMIC_SEQ_CST);
}

intptr_t aosl_hal_atomic_dec(intptr_t *v)
{
  return __atomic_fetch_sub((volatile intptr_t *)v, 1, __ATOMIC_SEQ_CST);
}

intptr_t aosl_hal_atomic_add(intptr_t i, intptr_t *v)
{
  return __atomic_add_fetch((volatile intptr_t *)v, i, __ATOMIC_SEQ_CST);
}

intptr_t aosl_hal_atomic_sub(intptr_t i, intptr_t *v)
{
  return __atomic_sub_fetch((volatile intptr_t *)v, i, __ATOMIC_SEQ_CST);
}

intptr_t aosl_hal_atomic_cmpxchg(intptr_t *v, intptr_t old, intptr_t new)
{
  if (__atomic_compare_exchange_n((volatile intptr_t *)v, &old, new, false,
    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
    return old;
  } else {
    return old;
  }
}

intptr_t aosl_hal_atomic_xchg(intptr_t *v, intptr_t new)
{
  return __atomic_exchange_n((volatile intptr_t *)v, new, __ATOMIC_SEQ_CST);
}

void aosl_hal_mb()
{
  __sync_synchronize();
}

void aosl_hal_rmb()
{
  __sync_synchronize();
}

void aosl_hal_wmb()
{
  __sync_synchronize();
}
