/*
 * Common.h - GoldHEN Plugins SDK Common Header
 * From: GoldHEN_Plugins_SDK/include/Common.h
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>

#include <orbis/libkernel.h>

#include "Detour.h"
#include "GoldHEN.h"
#include "Utilities.h"
#include "Syscall.h"

#define STRINGIFY(x)      #x
#define STRINGIFY_DEEP(x) STRINGIFY(x)

#ifdef __cplusplus
}
#endif
