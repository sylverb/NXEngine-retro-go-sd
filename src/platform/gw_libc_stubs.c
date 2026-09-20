/* Missing newlib pieces for NXEngine freestanding link. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

char *strerror(int errnum)
{
    (void)errnum;
    return (char *)"error";
}

int fflush(FILE *stream)
{
    (void)stream;
    return 0;
}

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p)
        memcpy(p, s, n);
    return p;
}

double atof(const char *nptr)
{
    return strtod(nptr, NULL);
}

int usleep(useconds_t usec)
{
    /* Approximate with HAL_Delay; wdog kicked by SDL_Delay paths usually. */
    extern void HAL_Delay(uint32_t ms);
    extern void wdog_refresh(void);
    uint32_t ms = (uint32_t)((usec + 999) / 1000);
    if (ms == 0)
        ms = 1;
    wdog_refresh();
    HAL_Delay(ms);
    return 0;
}

int rename(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    errno = EXDEV;
    return -1;
}
