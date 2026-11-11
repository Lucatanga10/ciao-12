/*
 * plugin_common.h - GoldHEN Plugin Common Header
 * From: GoldHEN_Plugins_Repository/common/plugin_common.h
 * Needed by fn_console_unlocker (sploit.sprx)
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <Common.h>

/* ---- Paths ---- */
#define GOLDHEN_PATH "/data/GoldHEN"
#define TEX_ICON_SYSTEM "cxml://psnotification/tex_icon_system"
#define MAX_PATH_ 260

/* ---- Type aliases ---- */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

/* ---- Attributes ---- */
#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public        __attribute__((visibility("default")))

/* ---- Build type ---- */
#if __FINAL__ == 1
  #define BUILD_TYPE "(Release)"
  #define debug_printf(a, args...)
#else
  #define BUILD_TYPE "(Debug)"
  #define debug_printf(a, args...) klog("[%s:%s:%d] " a, __func__, __FILE__, __LINE__, ##args)
#endif

/* ---- Logging ---- */
#define final_printf(a, args...) klog("[%s:%d] " a, __FILE__, __LINE__, ##args)

#define boot_ver() do { \
    final_printf("commit: %s branch: %s #%s %s\n", COMMIT_VER, BRANCH, BUILD_NUM, BUILD_TYPE); \
    final_printf("built: %s\n", BUILD_DATE); \
    final_printf("GoldHEN SDK ver: 0x%08x\n", GOLDHEN_SDK_VERSION); \
    final_printf("System SDK ver: 0x%08x\n", sys_sdk_version()); \
} while(0)

#define print_proc_info() do { \
    final_printf("pid: %d name: %s path: %s titleid: %s contentid: %s version: %s base: 0x%lx\n", \
        procInfo.pid, procInfo.name, procInfo.path, procInfo.titleid, \
        procInfo.contentid, procInfo.version, procInfo.base_address); \
} while(0)

/* ---- String helpers ---- */
#define startsWith(str1, str2) (strncmp(str1, str2, __builtin_strlen(str2)) == 0)

/* ---- Notification ---- */
void NotifyStatic(const char *IconUri, const char *text);
void Notify(const char *IconUri, const char *FMT, ...);

#ifdef __cplusplus
}
#endif
