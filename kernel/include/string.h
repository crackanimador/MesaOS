#ifndef STRING_H
#define STRING_H

#include <stddef.h>

extern "C" {
    void* memset(void* bufptr, int value, size_t size);
    void* memcpy(void* dstptr, const void* srcptr, size_t size);
    size_t strlen(const char* str);
    int memcmp(const void* s1, const void* s2, size_t n);
    int strcmp(const char* s1, const char* s2);
    int strncmp(const char* s1, const char* s2, size_t n);
    char* strcpy(char* dest, const char* src);
    char* strncpy(char* dest, const char* src, size_t n);
    char* strcat(char* dest, const char* src);
    char* strncat(char* dest, const char* src, size_t n);
    char* strchr(const char* s, int c);
    char* itoa(int value, char* str, int base);
    void* memmove(void* dest, const void* src, size_t n);
}

#endif
