#include "ipc.h"

#include "protocol.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void ipc_init(ipc_link *link)
{
    if (link == NULL) {
        return;
    }
    link->fd = -1;
    link->screen_width = 0;
    link->screen_height = 0;
}

static bool read_fully(int fd, uint8_t *buffer, size_t length)
{
    size_t done = 0;

    while (done < length) {
        ssize_t got = read(fd, buffer + done, length - done);
        if (got == 0) {
            return false;
        }
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        done += (size_t)got;
    }

    return true;
}

bool ipc_connect(ipc_link *link, const char *socket_name)
{
    struct sockaddr_un address;
    socklen_t address_len;
    size_t name_len;
    uint8_t hello[OJBS_HELLO_SIZE];
    int fd;

    if (link == NULL || socket_name == NULL) {
        return false;
    }

    ipc_close(link);

    name_len = strlen(socket_name);
    if (name_len == 0 || name_len + 1 > sizeof(address.sun_path)) {
        return false;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    /* abstract namespace: primo byte nullo, poi il nome, senza terminatore */
    address.sun_path[0] = '\0';
    memcpy(address.sun_path + 1, socket_name, name_len);
    address_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + name_len);

    if (connect(fd, (struct sockaddr *)&address, address_len) != 0) {
        close(fd);
        return false;
    }

    if (!read_fully(fd, hello, sizeof(hello))) {
        close(fd);
        return false;
    }

    if (!proto_decode_hello(hello, sizeof(hello),
                            &link->screen_width, &link->screen_height)) {
        close(fd);
        return false;
    }

    if (link->screen_width == 0 || link->screen_height == 0) {
        close(fd);
        return false;
    }

    link->fd = fd;
    return true;
}

bool ipc_connected(const ipc_link *link)
{
    return link != NULL && link->fd >= 0;
}

bool ipc_send(ipc_link *link, const uint8_t *data, size_t length)
{
    size_t done = 0;

    if (!ipc_connected(link) || data == NULL || length == 0) {
        return false;
    }

    while (done < length) {
        ssize_t sent = send(link->fd, data + done, length - done, MSG_NOSIGNAL);
        if (sent <= 0) {
            if (sent < 0 && errno == EINTR) {
                continue;
            }
            ipc_close(link);
            return false;
        }
        done += (size_t)sent;
    }

    return true;
}

void ipc_close(ipc_link *link)
{
    if (link == NULL) {
        return;
    }
    if (link->fd >= 0) {
        close(link->fd);
    }
    link->fd = -1;
    link->screen_width = 0;
    link->screen_height = 0;
}
