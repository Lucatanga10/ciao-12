/*
 * orbis/libkernel.h - PS4 Kernel API Stub Header
 * Minimal definitions needed for fn_console_unlocker compilation.
 * From: OpenOrbis PS4 Toolchain /include/orbis/libkernel.h
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/* ---- Kernel Types ---- */
typedef int32_t  OrbisKernelModule;
typedef uint32_t OrbisUserServiceUserId;

/* ---- Module Info ---- */
typedef struct {
    uint64_t address;
    uint32_t size;
    uint32_t prot;
} OrbisKernelModuleSegmentInfo;

typedef struct {
    size_t size;                               /* set to sizeof before calling */
    char name[256];
    OrbisKernelModuleSegmentInfo segmentInfo[4];
    uint32_t segmentCount;
    uint8_t fingerprint[20];
} OrbisKernelModuleInfo;

/* ---- Notification ---- */
typedef struct {
    int32_t  type;
    int32_t  unk3;
    int32_t  useIconImageUri;
    int32_t  targetId;
    char     message[1024];
    char     iconUri[1024];
    char     unk[1024];
} OrbisNotificationRequest;

/* ---- Kernel Functions ---- */
int  sceKernelSendNotificationRequest(int device, OrbisNotificationRequest *req,
                                       size_t size, int blocking);

int  sceKernelGetModuleInfo(int pid, OrbisKernelModule handle,
                             OrbisKernelModuleInfo *info);
int  sceKernelGetModuleList(int pid, OrbisKernelModule *handles, size_t *num);

int  sceKernelOpen(const char *path, int flags, int mode);
int  sceKernelClose(int fd);
ssize_t sceKernelRead(int fd, void *buf, size_t nbytes);
ssize_t sceKernelWrite(int fd, const void *buf, size_t nbytes);

int  sceKernelStat(const char *path, void *sb);
int  sceKernelUnlink(const char *path);
int  sceKernelRename(const char *from, const char *to);

int  sceKernelUsleep(unsigned int usec);

OrbisKernelModule sceKernelLoadStartModule(const char *path,
    size_t argc, const void *argv, uint32_t flags, void *opt, int *res);

int  sceKernelMprotect(void *addr, size_t len, int prot);
void *sceKernelMmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int  sceKernelMunmap(void *addr, size_t len);

int  sceKernelJitCreateSharedMemory(int flags, size_t size, int prot, int *fdOut);
int  sceKernelJitMapSharedMemory(int fd, int flags, void **addrOut,
                                  size_t size, int prot, int maxProt);

int  sceKernelDprintf(int fd, const char *fmt, ...);
int  sceKernelGetProcParam(void);
uint64_t sceKernelGetCpuFrequency(void);
int  sceKernelGetCurrentCpu(void);
size_t sceKernelGetDirectMemorySize(void);

int  scePthreadCreate(void **thread, const void *attr,
                       void *(*start_routine)(void*), void *arg);

/* ---- User Service ---- */
int  sceUserServiceGetInitialUser(OrbisUserServiceUserId *userId);

/* ---- O_FLAGS (subset) ---- */
#ifndef O_RDONLY
#define O_RDONLY  0x0000
#define O_WRONLY  0x0001
#define O_RDWR    0x0002
#define O_CREAT   0x0200
#define O_TRUNC   0x0400
#endif

#ifdef __cplusplus
}
#endif
