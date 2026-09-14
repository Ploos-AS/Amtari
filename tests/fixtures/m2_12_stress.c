/* M2.12: real GCC stress fixture with calls, pointers, locals and a loop. */
static __attribute__((noinline)) int sum4(const volatile int *p)
{
    int total = 0;
    int i;
    for (i = 0; i < 4; ++i) {
        total += p[i];
    }
    return total;
}

int amtari_entry(void)
{
    volatile int values[4];
    values[0] = 3;
    values[1] = 7;
    values[2] = 11;
    values[3] = 19;
    return sum4(values) + 2;
}
