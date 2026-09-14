/* M2.11: deliberately small freestanding C program compiled by real m68k GCC. */
int amtari_entry(void)
{
    volatile int left = 40;
    volatile int right = 2;
    return left + right;
}
