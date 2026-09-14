/* M2.12: real GCC stress fixture with calls, arguments, locals and a loop. */
static __attribute__((noinline)) int sum4(int a, int b, int c, int d, int count);

int amtari_entry(void)
{
    return sum4(3, 7, 11, 19, 4) + 2;
}

static __attribute__((noinline)) int sum4(int a, int b, int c, int d, int count)
{
    volatile int values[4];
    const volatile int *p;
    int total = 0;

    values[0] = a;
    values[1] = b;
    values[2] = c;
    values[3] = d;
    p = values;
    while (count-- > 0) {
        total += *p++;
    }
    return total;
}
