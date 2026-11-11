/*
 * orbis/NpTrophy.h - PS4 Trophy API Stub
 * Resolved at runtime via dlsym in sploit.sprx
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef int32_t OrbisNpTrophyContext;
typedef int32_t OrbisNpTrophyHandle;

typedef int (*sceNpTrophyCreateContext_t)(OrbisNpTrophyContext *context,
    int32_t userId, uint32_t serviceLabel, uint64_t options);
typedef int (*sceNpTrophyRegisterContext_t)(OrbisNpTrophyContext context,
    OrbisNpTrophyHandle handle, uint64_t options);
typedef int (*sceNpTrophyCreateHandle_t)(OrbisNpTrophyHandle *handle);
typedef int (*sceNpTrophyUnlockTrophy_t)(OrbisNpTrophyContext context,
    OrbisNpTrophyHandle handle, int32_t trophyId, int32_t *platinumId);
typedef int (*sceNpTrophyGetTrophyUnlockState_t)(OrbisNpTrophyContext context,
    OrbisNpTrophyHandle handle, void *flags, uint32_t *count);

#ifdef __cplusplus
}
#endif
