#include <stdio.h>
#include <stdint.h>

void fputl(uint32_t word, FILE *fp);

void fputl(unsigned int word, FILE *fp)
{
    fputl((uint32_t)word, fp);
}
