/* M2.13: force real GCC indexed addressing and callee-saved MOVEM traffic. */
static __attribute__((noinline)) int indexed_movem(const volatile signed char *p, int index)
{
    register int a __asm__("d2") = p[index];
    register int b __asm__("d3") = 1;
    register int c __asm__("d4") = 1;

    __asm__ volatile ("" : "+d" (a), "+d" (b), "+d" (c));
    return a + b + c;
}

int amtari_entry(void)
{
    volatile signed char values[8];
    values[5] = 40;
    return indexed_movem(values, 5);
}
