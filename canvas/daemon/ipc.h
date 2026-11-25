#ifndef OJBS_IPC_H
#define OJBS_IPC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Client sull'abstract unix socket dell'APK (@ojbs_esp). Abstract namespace:
 * nessun file su disco, nessun permesso da sistemare, e il nome vive finche'
 * vive il server.
 */

typedef struct {
    int fd;
    uint32_t screen_width;
    uint32_t screen_height;
} ipc_link;

void ipc_init(ipc_link *link);

/* Connette e legge l'hello con la dimensione della superficie overlay. */
bool ipc_connect(ipc_link *link, const char *socket_name);

bool ipc_connected(const ipc_link *link);

/* Scrittura completa: un invio parziale e' un fallimento e chiude il link. */
bool ipc_send(ipc_link *link, const uint8_t *data, size_t length);

void ipc_close(ipc_link *link);

#endif /* OJBS_IPC_H */
