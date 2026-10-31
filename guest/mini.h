// Biblioteca mínima de los programas de prueba: salida con formato por svcOutputDebugString
// (o write() en la versión de referencia para x86_64) y memset/memcpy para el modo libre.
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int64_t s64;

#ifdef NATIVE_REF
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void out_raw(const char *s, size_t n) { write(1, s, n); }
static u64 tick_counter;
static u64 ticks(void) { return ++tick_counter; }
#else
static void out_raw(const char *s, size_t n) {
    register const char *x0 __asm__("x0") = s;
    register size_t x1 __asm__("x1") = n;
    __asm__ volatile("svc #0x27" : "+r"(x0) : "r"(x1) : "memory"); // svcOutputDebugString
}
__attribute__((unused)) static u64 ticks(void) {
    register u64 x0 __asm__("x0");
    __asm__ volatile("svc #0x1e" : "=r"(x0) : : "memory"); // svcGetSystemTick
    return x0;
}
void *memset(void *d, int v, size_t n) {
    u8 *p = d;
    while (n--) *p++ = (u8)v;
    return d;
}
void *memcpy(void *d, const void *s, size_t n) {
    u8 *p = d;
    const u8 *q = s;
    while (n--) *p++ = *q++;
    return d;
}
#endif

// ---------- salida con formato ----------
static char buf[512];
static size_t blen;

static void flush(void) {
    if (blen) out_raw(buf, blen);
    blen = 0;
}
static void put(char c) {
    if (blen == sizeof buf) flush();
    buf[blen++] = c;
}
static void put_u(u64 v, unsigned base) {
    char t[24];
    int i = 0;
    do {
        t[i++] = "0123456789abcdef"[v % base];
        v /= base;
    } while (v);
    while (i) put(t[--i]);
}
static void put_s(s64 v) {
    if (v < 0) {
        put('-');
        put_u((u64)0 - (u64)v, 10);
    } else {
        put_u((u64)v, 10);
    }
}

// %d s32, %u u32, %x u32 hex, %D s64, %U u64, %X u64 hex, %s cadena, %c carácter
static void pr(const char *f, ...) {
    va_list ap;
    va_start(ap, f);
    for (; *f; f++) {
        if (*f != '%') {
            put(*f);
            continue;
        }
        switch (*++f) {
        case 'd': put_s(va_arg(ap, s32)); break;
        case 'u': put_u(va_arg(ap, u32), 10); break;
        case 'x': put_u(va_arg(ap, u32), 16); break;
        case 'D': put_s(va_arg(ap, s64)); break;
        case 'U': put_u(va_arg(ap, u64), 10); break;
        case 'X': put_u(va_arg(ap, u64), 16); break;
        case 's': for (const char *s = va_arg(ap, const char *); *s; s++) put(*s); break;
        case 'c': put((char)va_arg(ap, int)); break;
        case '%': put('%'); break;
        default: put('%'); put(*f); break;
        }
    }
    va_end(ap);
}
