// fsp-srv: sistema de archivos. La tarjeta SD es una carpeta del anfitrión
// (NX_SDMC, por defecto ~/.local/share/nxrecomp/sdmc) y el RomFS del juego es una imagen
// (NX_ROMFS) que se lee como IStorage. Las rutas las elige el programa que corre, así que no
// pueden salir de la raíz (nada de "..") y nunca pasan por un shell.
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "nxkern.h"
#include "nxsvc.h"

#define OK 0
#define R_FS_NOT_FOUND 0x202u        // fs, 1
#define R_FS_EXISTS 0x402u           // fs, 2
#define R_FS_OUT_OF_RANGE 0x2EE202u  // fs, 6001
#define R_FS_INVALID_PATH 0x2EE602u  // fs, 6003
#define R_FS_NOT_EMPTY 0x13F202u     // fs, 2551

// ---------- RomFS como IStorage ----------
typedef struct {
    FILE *f;
    uint64_t size;
} Storage;

static uint32_t storage_handle(ServiceObject *self, NxRequest *r) {
    Storage *s = self->state;
    switch (r->cmd) {
    case 0: { // Read(offset, size) → buffer B
        uint64_t off = nx_in64(r, 0), size = nx_in64(r, 8);
        if (!r->nb || size > r->b[0].size || off > s->size || size > s->size - off) return R_FS_OUT_OF_RANGE;
        fseeko(s->f, (off_t)off, SEEK_SET);
        if (fread((void *)(uintptr_t)r->b[0].addr, 1, size, s->f) != size) return R_FS_OUT_OF_RANGE;
        return OK;
    }
    case 4: nx_out64(r, s->size); return OK; // GetSize
    }
    return NX_R_UNKNOWN_CMD;
}

// ---------- la SD como carpeta ----------
typedef struct {
    char root[1024];
} FsRoot;

// Ruta del anfitrión a partir de la del programa (una cadena en un buffer X).
static int host_path(FsRoot *fs, NxBuffer *x, char *out, size_t n) {
    if (!x || !x->addr) return -1;
    char p[0x302] = {0};
    memcpy(p, (const void *)(uintptr_t)x->addr, x->size < 0x301 ? x->size : 0x301);
    if (strstr(p, "..")) return -1;
    snprintf(out, n, "%s%s%s", fs->root, p[0] == '/' ? "" : "/", p);
    return 0;
}

static uint32_t errno_result(void) {
    if (errno == ENOENT || errno == ENOTDIR) return R_FS_NOT_FOUND;
    if (errno == EEXIST) return R_FS_EXISTS;
    if (errno == ENOTEMPTY) return R_FS_NOT_EMPTY;
    return R_FS_INVALID_PATH;
}

static const char *clean_root; // para no borrar la carpeta misma en CleanDirectoryRecursively
static int remove_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw) {
    (void)st, (void)flag, (void)ftw;
    if (clean_root && !strcmp(path, clean_root)) return 0;
    return remove(path);
}
static int remove_tree(const char *path, int keep_root) {
    clean_root = keep_root ? path : NULL;
    int rc = nftw(path, remove_entry, 32, FTW_DEPTH | FTW_PHYS); // FTW_PHYS: no sigue enlaces simbólicos
    clean_root = NULL;
    return rc;
}

static int mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *s = tmp + 1; *s; s++)
        if (*s == '/') {
            *s = 0;
            if (mkdir(tmp, 0755) && errno != EEXIST) return -1;
            *s = '/';
        }
    return mkdir(tmp, 0755) && errno != EEXIST ? -1 : 0;
}

static uint32_t file_handle(ServiceObject *self, NxRequest *r) {
    int fd = (int)(intptr_t)self->state;
    switch (r->cmd) {
    case 0: { // Read(opción, offset, tamaño) → buffer B, bytes leídos
        uint64_t off = nx_in64(r, 8), size = nx_in64(r, 16);
        if (!r->nb) return R_FS_OUT_OF_RANGE;
        if (size > r->b[0].size) size = r->b[0].size;
        ssize_t got = pread(fd, (void *)(uintptr_t)r->b[0].addr, size, (off_t)off);
        nx_out64(r, got < 0 ? 0 : (uint64_t)got);
        return got < 0 ? R_FS_OUT_OF_RANGE : OK;
    }
    case 1: { // Write(opción, offset, tamaño) ← buffer A
        uint64_t off = nx_in64(r, 8), size = nx_in64(r, 16);
        if (!r->na || size > r->a[0].size) return R_FS_OUT_OF_RANGE;
        return pwrite(fd, (const void *)(uintptr_t)r->a[0].addr, size, (off_t)off) == (ssize_t)size ? OK : R_FS_OUT_OF_RANGE;
    }
    case 2: fsync(fd); return OK;                                        // Flush
    case 3: return ftruncate(fd, (off_t)nx_in64(r, 0)) ? errno_result() : OK; // SetSize
    case 4: { struct stat st; fstat(fd, &st); nx_out64(r, (uint64_t)st.st_size); return OK; } // GetSize
    }
    return NX_R_UNKNOWN_CMD;
}

typedef struct {
    DIR *d;
    int mode; // 1 directorios, 2 archivos
    char path[1024];
} Dir;

static int dir_next(Dir *d, struct dirent **out, struct stat *st) {
    struct dirent *e;
    while ((e = readdir(d->d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[2048];
        snprintf(full, sizeof full, "%s/%s", d->path, e->d_name);
        if (stat(full, st)) continue;
        int is_dir = S_ISDIR(st->st_mode);
        if ((is_dir && !(d->mode & 1)) || (!is_dir && !(d->mode & 2))) continue;
        *out = e;
        return 1;
    }
    return 0;
}

static uint32_t dir_handle(ServiceObject *self, NxRequest *r) {
    Dir *d = self->state;
    switch (r->cmd) {
    case 0: { // Read → entradas de 0x310 bytes en el buffer B
        if (!r->nb) return R_FS_OUT_OF_RANGE;
        uint64_t max = r->b[0].size / 0x310, n = 0;
        struct dirent *e;
        struct stat st;
        while (n < max && dir_next(d, &e, &st)) {
            uint8_t *ent = (uint8_t *)(uintptr_t)(r->b[0].addr + n * 0x310);
            memset(ent, 0, 0x310);
            strncpy((char *)ent, e->d_name, 0x300);
            ent[0x304] = S_ISDIR(st.st_mode) ? 0 : 1;
            int64_t size = S_ISDIR(st.st_mode) ? 0 : (int64_t)st.st_size;
            memcpy(ent + 0x308, &size, 8);
            n++;
        }
        nx_out64(r, n);
        return OK;
    }
    case 1: { // GetEntryCount
        Dir tmp = *d;
        tmp.d = opendir(d->path);
        uint64_t n = 0;
        struct dirent *e;
        struct stat st;
        while (tmp.d && dir_next(&tmp, &e, &st)) n++;
        if (tmp.d) closedir(tmp.d);
        nx_out64(r, n);
        return OK;
    }
    }
    return NX_R_UNKNOWN_CMD;
}

static uint32_t filesystem_handle(ServiceObject *self, NxRequest *r) {
    FsRoot *fs = self->state;
    char p[2048], q[2048];
    NxBuffer *x0 = r->nx ? &r->x[0] : NULL, *x1 = r->nx > 1 ? &r->x[1] : NULL;
    if ((r->cmd <= 9 || (r->cmd >= 11 && r->cmd <= 14)) && host_path(fs, x0, p, sizeof p)) return R_FS_INVALID_PATH;
    switch (r->cmd) {
    case 0: { // CreateFile(opción, tamaño)
        int fd = open(p, O_CREAT | O_EXCL | O_WRONLY, 0644);
        if (fd < 0) return errno_result();
        int rc = ftruncate(fd, (off_t)nx_in64(r, 8));
        close(fd);
        return rc ? errno_result() : OK;
    }
    case 1: return unlink(p) ? errno_result() : OK;
    case 2: return mkdir(p, 0755) ? errno_result() : OK;
    case 3: return rmdir(p) ? errno_result() : OK;
    case 4: return remove_tree(p, 0) ? errno_result() : OK;  // DeleteDirectoryRecursively
    case 13: return remove_tree(p, 1) ? errno_result() : OK; // CleanDirectoryRecursively
    case 5: case 6: // RenameFile / RenameDirectory
        if (host_path(fs, x1, q, sizeof q)) return R_FS_INVALID_PATH;
        return rename(p, q) ? errno_result() : OK;
    case 7: { // GetEntryType
        struct stat st;
        if (stat(p, &st)) return errno_result();
        nx_out32(r, S_ISDIR(st.st_mode) ? 0 : 1);
        return OK;
    }
    case 8: { // OpenFile(modo: 1 lectura, 2 escritura, 4 agregar)
        int fd = open(p, (nx_in32(r, 0) & 2) ? O_RDWR : O_RDONLY);
        if (fd < 0) return errno_result();
        nx_out_object(r, nx_object("IFile", file_handle, (void *)(intptr_t)fd));
        return OK;
    }
    case 9: { // OpenDirectory(modo)
        Dir *d = calloc(1, sizeof *d);
        d->d = opendir(p);
        if (!d->d) {
            free(d);
            return errno_result();
        }
        d->mode = (int)nx_in32(r, 0);
        snprintf(d->path, sizeof d->path, "%s", p);
        nx_out_object(r, nx_object("IDirectory", dir_handle, d));
        return OK;
    }
    case 10: return OK; // Commit
    case 11: case 12: { // GetFreeSpaceSize / GetTotalSpaceSize
        struct statvfs sv;
        if (statvfs(fs->root, &sv)) return errno_result();
        nx_out64(r, (uint64_t)sv.f_frsize * (r->cmd == 11 ? sv.f_bavail : sv.f_blocks));
        return OK;
    }
    case 14: { // GetFileTimeStampRaw
        struct stat st;
        if (stat(p, &st)) return errno_result();
        uint64_t ts[4] = {(uint64_t)st.st_ctime, (uint64_t)st.st_mtime, (uint64_t)st.st_atime, 1};
        nx_out_bytes(r, ts, sizeof ts);
        return OK;
    }
    }
    return NX_R_UNKNOWN_CMD;
}

static ServiceObject *sdcard(void) {
    FsRoot *fs = calloc(1, sizeof *fs);
    const char *env = getenv("NX_SDMC");
    if (env && *env) snprintf(fs->root, sizeof fs->root, "%s", env);
    else snprintf(fs->root, sizeof fs->root, "%s/.local/share/nxrecomp/sdmc", getenv("HOME") ? getenv("HOME") : ".");
    if (mkdir_p(fs->root)) return NULL;
    return nx_object("IFileSystem (SD)", filesystem_handle, fs);
}

uint32_t nx_fsp(ServiceObject *self, NxRequest *r) {
    (void)self;
    switch (r->cmd) {
    case 1: case 1003: case 1006: return OK; // SetCurrentProcess, DisableAutoSaveDataCreation, OutputAccessLogToSdCard
    case 18: { // OpenSdCardFileSystem
        ServiceObject *o = sdcard();
        if (!o) return R_FS_INVALID_PATH;
        nx_out_object(r, o);
        return OK;
    }
    case 200: { // OpenDataStorageByCurrentProcess: el RomFS del juego
        const char *path = getenv("NX_ROMFS");
        FILE *f = path ? fopen(path, "rb") : NULL;
        if (!f) {
            fprintf(stderr, "[RED] el juego abrió su RomFS, pero no hay imagen: define NX_ROMFS=<RomFS.img>\n");
            return 0x7D402u;
        }
        Storage *s = calloc(1, sizeof *s);
        s->f = f;
        fseeko(f, 0, SEEK_END);
        s->size = (uint64_t)ftello(f);
        nx_out_object(r, nx_object("IStorage (RomFS)", storage_handle, s));
        return OK;
    }
    case 1005: nx_out32(r, 0); return OK; // GetGlobalAccessLogMode
    }
    return NX_R_UNKNOWN_CMD;
}
