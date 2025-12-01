#ifndef KERNEL_STRING_H
#define KERNEL_STRING_H

#include <stddef.h> /* size_t */

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);

size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
int strcmp(const char *a, const char *b);

#endif /* KERNEL_STRING_H */
