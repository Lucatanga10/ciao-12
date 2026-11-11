/*
 * Utilities.h - GoldHEN Plugins SDK Utilities
 * From: GoldHEN_Plugins_SDK/include/Utilities.h
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef unsigned char vm_prot_t;
typedef unsigned long HA;

#define ARRAY_COUNT(arry) (sizeof(arry) / sizeof(arry[0]))
#define klog printf

/* Memory protection */
#define VM_PROT_NONE    ((vm_prot_t) 0x00)
#define VM_PROT_READ    ((vm_prot_t) 0x01)
#define VM_PROT_WRITE   ((vm_prot_t) 0x02)
#define VM_PROT_EXECUTE ((vm_prot_t) 0x04)
#define VM_PROT_COPY    ((vm_prot_t) 0x08)
#define VM_PROT_ALL     (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE)
#define VM_PROT_RW      (VM_PROT_READ | VM_PROT_WRITE)
#define VM_PROT_DEFAULT VM_PROT_RW

/* Mount */
#define MNT_UPDATE 0x00010000

/* Hooking macros */
#define HA_MANYARGS HA arg1, HA arg2, HA arg3, HA arg4, HA arg5, HA arg6, \
    HA arg7, HA arg8, HA arg9, HA arg10, HA arg11, HA arg12
#define MANYARGS arg1, arg2, arg3, arg4, arg5, arg6, \
    arg7, arg8, arg9, arg10, arg11, arg12

#define HOOK_EXTERN(name) extern Detour Detour_##name
#define HOOK_INIT(name)   Detour Detour_##name = { .Mode = DetourMode_x64 }

#define HOOK(name) do { \
    Detour_Construct(&Detour_##name, DetourMode_x64); \
    Detour_DetourFunction(&Detour_##name, (uint64_t)name, (void *)name##_hook); \
} while(0)

#define HOOK32(name) do { \
    Detour_Construct(&Detour_##name, DetourMode_x32); \
    Detour_DetourFunction(&Detour_##name, (uint64_t)name, (void *)name##_hook); \
} while(0)

#define HOOK_CONTINUE(name, type, ...) \
    Detour_Stub(&Detour_##name, type, __VA_ARGS__)

#define UNHOOK(name) Detour_Destroy(&Detour_##name)

/* hex dump */
void hex_dump(void *data, size_t size);

#ifdef __cplusplus
}
#endif
