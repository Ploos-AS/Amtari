/* M2.14: exercise linked .data and .bss through real GCC output. */
volatile int m214_initialized = 40;
volatile int m214_zeroed;

int amtari_entry(void)
{
    m214_zeroed = 2;
    return m214_initialized + m214_zeroed;
}
