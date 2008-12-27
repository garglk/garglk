#include <ctype.h>

/*
 *   Provide memicmp, since it's not a standard libc routine.  
 */
int os_memicmp(const char *s1, const char *s2, int len)
{
    int i;

    for (i = 0; i < len; i++)
    {
        if (tolower(s1[i]) != tolower(s2[i]))
            return (int)tolower(s1[i]) - (int)tolower(s2[i]);
    }
    return 0;
}

