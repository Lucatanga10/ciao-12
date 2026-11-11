/*
 * GoldHEN.h - GoldHEN SDK Header
 * From: GoldHEN_Plugins_SDK/include/GoldHEN.h
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ---- Structures ---- */
typedef struct {
    uint64_t cr_uid;
    uint64_t cr_caps;
    uint64_t cr_prison;
    uint64_t fd_cdir;
    uint64_t fd_rdir;
    uint64_t fd_jdir;
} jailbreak_backup;

typedef struct {
    int32_t  pid;
    char     name[32];
    char     path[256];
    char     titleid[16];
    char     contentid[48];
    char     version[8];
    uint64_t base_address;
} proc_info;

typedef struct {
    uint64_t address;
    void    *data;
    uint64_t length;
    uint32_t write;
} proc_rw;

typedef struct {
    char    name[32];
    char    path[256];
    int32_t result;
} proc_prx_load;

typedef struct {
    char    name[32];
    int32_t handle;
    int32_t result;
} proc_prx_unload;

/* ---- Command IDs ---- */
#define GOLDHEN_SDK_CMD_VERSION          0
#define GOLDHEN_SDK_CMD_KLOG             1
#define GOLDHEN_SDK_CMD_JAILBREAK        2
#define GOLDHEN_SDK_CMD_UNJAILBREAK      3
#define GOLDHEN_SDK_CMD_PROCESS_INFO     4
#define GOLDHEN_SDK_CMD_PROCESS_RW       5
#define GOLDHEN_SDK_CMD_PROCESS_PRX_LOAD   6
#define GOLDHEN_SDK_CMD_PROCESS_PRX_UNLOAD 7

/* ---- Version ---- */
#define GOLDHEN_SDK_VERSION 0x00000100 /* 1.00 */

/* ---- Function Declarations ---- */
int      sys_sdk_cmd(uint64_t cmd, void *data);
int      sys_sdk_jailbreak(jailbreak_backup *backup);
int      sys_sdk_unjailbreak(jailbreak_backup *backup);
uint32_t sys_sdk_version(void);
int      sys_sdk_proc_info(proc_info *info);
int      sys_sdk_proc_rw(proc_rw *rw);
int      sys_sdk_proc_prx_load(proc_prx_load *load);
int      sys_sdk_proc_prx_unload(proc_prx_unload *unload);

#ifdef __cplusplus
}
#endif
