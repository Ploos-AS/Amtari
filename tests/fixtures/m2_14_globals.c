/* M2.14: exercise linked .data and .bss through real GCC output. */
volatile int m214_initialized = 40;
volatile int m214_zeroed;

int amtari_entry(void)
{
    volatile int *initialized = &m214_initialized;
    volatile int *zeroed = &m214_zeroed;

    __asm__ volatile ("" : "+a" (initialized), "+a" (zeroed));
    *zeroed = 2;
    return *initialized + *zeroed;
}
