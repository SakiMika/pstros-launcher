#ifndef KVM_STDIO_REDIRECT_H
#define KVM_STDIO_REDIRECT_H

/*
 * Load the real C stdio declarations before defining KVM's output redirects.
 *
 * The old Makefile used -Dprintf=printf_md.  That object-like macro also
 * changed Calico/libnds declarations such as:
 *
 *     __attribute__((format(printf, 1, 2)))
 *
 * into format(printf_md,...), which GCC does not recognise.  Function-like
 * macros only expand at actual calls (printf(...)), not inside format
 * attributes, so platform headers remain intact.
 */
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void printf_md(const char *format, ...);
void fprintf_md(FILE *stream, const char *format, ...);
int sprintf_md(char *buffer, const char *format, ...);
void vprintf_md(const char *format, va_list arguments);
int vsnprintf_md(char *buffer, size_t size, const char *format,
                 va_list arguments);

#ifdef __cplusplus
}
#endif

#ifndef KVM_STDIO_IMPLEMENTATION
#define printf(...)        printf_md(__VA_ARGS__)
#define fprintf(...)       fprintf_md(__VA_ARGS__)
#define sprintf(...)       sprintf_md(__VA_ARGS__)
#endif

#endif /* KVM_STDIO_REDIRECT_H */
