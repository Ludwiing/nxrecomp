// time: relojes y zona horaria, con la hora real del sistema anfitrión.
// Desde el sistema 6.0.0 el juego lee los relojes de una memoria compartida:
//   0x00 reloj continuo (offset interno en ns + id de la fuente)
//   0x38 contexto del reloj local   (segundos POSIX = offset + instante del reloj continuo)
//   0x80 contexto del reloj de red
//   0xC8 corrección automática activada
// Cada objeto empieza con un contador; los datos van en dos copias y se lee la del bit bajo.
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nxkern.h"
#include "nxsvc.h"

#define OK 0

static const uint8_t source_id[16] = {0x6e, 0x78, 0x72, 0x65, 0x63, 0x6f, 0x6d, 0x70, 1, 2, 3, 4, 5, 6, 7, 8};
static KShmem *time_shmem;
static uint32_t time_shmem_h;

static int64_t steady_seconds(void) { return (int64_t)(nx_now_ns() / 1000000000ull); }

typedef struct {
    uint64_t offset;
    int64_t time_point;
    uint8_t source[16];
} ClockContext;

static ClockContext context_now(void) {
    ClockContext c;
    c.time_point = steady_seconds();
    c.offset = (uint64_t)(time(NULL) - c.time_point);
    memcpy(c.source, source_id, 16);
    return c;
}

static void shmem_write(uint8_t *base, size_t off, const void *obj, size_t size) {
    memcpy(base + off + 8, obj, size); // copia 0, con el contador en 0
    memcpy(base + off + 8 + size, obj, size);
}

static void ensure_shmem(void) {
    if (time_shmem_h) return;
    time_shmem_h = nx_shmem_new(0x1000, &time_shmem);
    uint8_t steady[0x18] = {0};
    memcpy(steady + 8, source_id, 16); // offset interno 0: el reloj continuo son los ticks del sistema
    shmem_write(time_shmem->host, 0x00, steady, sizeof steady);
    ClockContext c = context_now();
    shmem_write(time_shmem->host, 0x38, &c, sizeof c);
    shmem_write(time_shmem->host, 0x80, &c, sizeof c);
    uint8_t automatic = 0;
    shmem_write(time_shmem->host, 0xC8, &automatic, 1);
}

static uint32_t system_clock(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: nx_out64(r, (uint64_t)time(NULL)); return OK;   // GetCurrentTime
    case 1: case 3: return OK;                               // SetCurrentTime, SetSystemClockContext
    case 2: { ClockContext c = context_now(); nx_out_bytes(r, &c, sizeof c); return OK; } // GetSystemClockContext
    case 4: { KEvent *e; nx_out_copy_handle(r, nx_event_new(&e)); return OK; }
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t steady_clock(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: { // GetCurrentTimePoint
        int64_t s = steady_seconds();
        nx_out_bytes(r, &s, 8);
        nx_out_bytes(r, source_id, 16);
        return OK;
    }
    case 2: case 200: nx_out64(r, 0); return OK; // GetTestOffset, GetInternalOffset
    case 100: nx_out64(r, (uint64_t)time(NULL)); return OK; // GetRtcValue
    }
    return NX_R_UNKNOWN_CMD;
}

typedef struct {
    int16_t year;
    int8_t month, day, hour, minute, second, pad;
} CalendarTime;
typedef struct {
    uint32_t day_of_week, day_of_year;
    char tz_name[8];
    uint32_t is_dst;
    int32_t utc_offset;
} CalendarInfo;

static void calendar(uint64_t posix, CalendarTime *ct, CalendarInfo *ci) {
    time_t t = (time_t)posix;
    struct tm tm;
    localtime_r(&t, &tm);
    *ct = (CalendarTime){(int16_t)(tm.tm_year + 1900), (int8_t)(tm.tm_mon + 1), (int8_t)tm.tm_mday,
                         (int8_t)tm.tm_hour, (int8_t)tm.tm_min, (int8_t)tm.tm_sec, 0};
    memset(ci, 0, sizeof *ci);
    ci->day_of_week = (uint32_t)tm.tm_wday;
    ci->day_of_year = (uint32_t)tm.tm_yday;
    strncpy(ci->tz_name, tm.tm_zone ? tm.tm_zone : "UTC", 7);
    ci->is_dst = tm.tm_isdst > 0;
    ci->utc_offset = (int32_t)tm.tm_gmtoff;
}

static uint32_t timezone_service(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: { // GetDeviceLocationName
        char name[0x24] = {0};
        const char *tz = getenv("TZ");
        strncpy(name, tz && *tz ? tz : "America/Bogota", sizeof name - 1);
        nx_out_bytes(r, name, sizeof name);
        return OK;
    }
    case 100: case 101: { // ToCalendarTime(WithMyRule)
        CalendarTime ct;
        CalendarInfo ci;
        calendar(nx_in64(r, 0), &ct, &ci);
        nx_out_bytes(r, &ct, sizeof ct);
        nx_out_bytes(r, &ci, sizeof ci);
        return OK;
    }
    case 201: case 202: { // ToPosixTime(WithMyRule): CalendarTime → lista en el buffer de salida
        CalendarTime ct;
        memcpy(&ct, r->in, sizeof ct);
        struct tm tm = {.tm_year = ct.year - 1900, .tm_mon = ct.month - 1, .tm_mday = ct.day,
                        .tm_hour = ct.hour, .tm_min = ct.minute, .tm_sec = ct.second, .tm_isdst = -1};
        int64_t t = (int64_t)mktime(&tm);
        NxBuffer *b = r->nc ? &r->c[0] : r->nb ? &r->b[0] : NULL;
        if (b && b->size >= 8) memcpy((void *)(uintptr_t)b->addr, &t, 8);
        nx_out32(r, b ? 1 : 0);
        return OK;
    }
    }
    return NX_R_UNKNOWN_CMD;
}

uint32_t nx_time(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 0: case 1: case 4: // relojes de usuario, de red y local
        nx_out_object(r, nx_object("ISystemClock", system_clock, NULL));
        return OK;
    case 2: nx_out_object(r, nx_object("ISteadyClock", steady_clock, NULL)); return OK;
    case 3: nx_out_object(r, nx_object("ITimeZoneService", timezone_service, NULL)); return OK;
    case 5: nx_out_object(r, nx_object("IEphemeralNetworkSystemClock", system_clock, NULL)); return OK;
    case 20: ensure_shmem(); nx_out_copy_handle(r, time_shmem_h); return OK; // GetSharedMemoryNativeHandle
    case 100: nx_out8(r, 0); return OK;       // IsStandardUserSystemClockAutomaticCorrectionEnabled
    case 101: return OK;
    case 200: nx_out8(r, 1); return OK;       // IsStandardNetworkSystemClockAccuracySufficient
    }
    return NX_R_UNKNOWN_CMD;
}
