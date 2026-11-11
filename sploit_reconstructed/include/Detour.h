/*
 * Detour.h - GoldHEN Plugins SDK Detour/Hook Header
 * From: GoldHEN_Plugins_SDK/include/Detour.h
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum _GHSDK_DetourMode {
    DetourMode_x64,
    DetourMode_x32
} DetourMode;

typedef struct _GHSDK_Detour {
    DetourMode Mode;
    void      *StubPtr;
    size_t     StubSize;
    void      *FunctionPtr;
    void      *TrampolinePtr;
    void      *HookPtr;
    uint8_t    JumpInstructions64[14];
    uint8_t    JumpInstructions32[5];
} Detour;

#define Detour_Stub(This, FunctionPointerType, ...) \
    (((FunctionPointerType)((This)->StubPtr))(__VA_ARGS__))

void *Detour_DetourFunction(Detour *This, uint64_t FunctionPtr, void *HookPtr);
void  Detour_RestoreFunction(Detour *This);
void  Detour_Construct(Detour *This, DetourMode Mode);
void  Detour_Destroy(Detour *This);

#ifdef __cplusplus
}
#endif
