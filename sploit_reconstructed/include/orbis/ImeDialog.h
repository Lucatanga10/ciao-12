/*
 * orbis/ImeDialog.h - PS4 IME (Keyboard) Dialog Stub
 * Resolved at runtime via LoadStartModule in sploit.sprx
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <wchar.h>

typedef struct {
    uint32_t userId;
    int32_t  type;
    uint64_t supportedLanguages;
    int32_t  enterLabel;
    int32_t  inputMethod;
    void    *filter;
    uint32_t option;
    uint32_t maxTextLength;
    wchar_t *inputTextBuffer;
    float    posx;
    float    posy;
    int32_t  horizontalAlignment;
    int32_t  verticalAlignment;
    wchar_t *placeholder;
    wchar_t *title;
    uint8_t  reserved[16];
} OrbisImeDialogParam;

typedef struct {
    int32_t  endstatus;
    uint8_t  reserved[12];
} OrbisImeDialogResult;

typedef int (*sceImeDialogInit_t)(OrbisImeDialogParam *param, void *extended);
typedef int (*sceImeDialogGetStatus_t)(void);
typedef int (*sceImeDialogGetResult_t)(OrbisImeDialogResult *result);
typedef int (*sceImeDialogTerm_t)(void);

#ifdef __cplusplus
}
#endif
