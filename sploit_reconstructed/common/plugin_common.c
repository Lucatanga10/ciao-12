/*
 * plugin_common.c - GoldHEN Plugin Common Implementation
 * From: GoldHEN_Plugins_Repository/common/plugin_common.c
 * Credits: OSM (PS4-Notify)
 */
#include "plugin_common.h"

void NotifyStatic(const char *IconUri, const char *text)
{
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    final_printf("%s\n", text);

    req.type = 0;
    req.unk3 = 0;
    req.useIconImageUri = 1;
    req.targetId = -1;

    strncpy(req.message, text, sizeof(req.message) - 1);
    strncpy(req.iconUri, IconUri, sizeof(req.iconUri) - 1);

    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

void Notify(const char *IconUri, const char *FMT, ...)
{
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));

    va_list args;
    va_start(args, FMT);
    vsnprintf(req.message, sizeof(req.message), FMT, args);
    va_end(args);

    final_printf("%s\n", req.message);

    req.type = 0;
    req.unk3 = 0;
    req.useIconImageUri = 1;
    req.targetId = -1;

    strncpy(req.iconUri, IconUri, sizeof(req.iconUri) - 1);

    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}
