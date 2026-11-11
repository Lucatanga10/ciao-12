/*
 * Syscall.h - GoldHEN Plugins SDK Syscall Header
 * From: GoldHEN_Plugins_SDK/include/Syscall.h
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int orbis_syscall(int num, ...);
int sys_dynlib_dlsym(int loadedModuleID, const char *name, void *destination);
int sys_dynlib_load_prx(const char *name, int *idDestination);

#ifdef __cplusplus
}
#endif
