#define _GNU_SOURCE

#include "memread.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#define OJBS_USER_MIN 0x10000ULL
#define OJBS_USER_MAX 0x8000000000ULL

static bool all_digits(const char *text)
{
    size_t i;

    if (text == NULL || text[0] == '\0') {
        return false;
    }
    for (i = 0; text[i] != '\0'; ++i) {
        if (!isdigit((unsigned char)text[i])) {
            return false;
        }
    }
    return true;
}

pid_t mem_find_pid(const char *package)
{
    DIR *proc;
    struct dirent *entry;
    pid_t found = 0;

    if (package == NULL || package[0] == '\0') {
        return 0;
    }

    proc = opendir("/proc");
    if (proc == NULL) {
        return 0;
    }

    while ((entry = readdir(proc)) != NULL) {
        char path[64];
        char cmdline[256];
        int fd;
        ssize_t got;

        if (!all_digits(entry->d_name)) {
            continue;
        }

        snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            continue;
        }

        got = read(fd, cmdline, sizeof(cmdline) - 1);
        close(fd);

        if (got <= 0) {
            continue;
        }

        cmdline[got] = '\0';

        if (strcmp(cmdline, package) == 0) {
            found = (pid_t)atoi(entry->d_name);
            break;
        }
    }

    closedir(proc);
    return found;
}

bool mem_find_module(pid_t pid, const char *module_name, mem_target *out)
{
    char path[64];
    char line[512];
    FILE *maps;
    uint64_t low = 0;
    uint64_t high = 0;
    bool seen = false;

    if (module_name == NULL || out == NULL || pid <= 0) {
        return false;
    }

    snprintf(path, sizeof(path), "/proc/%d/maps", (int)pid);
    maps = fopen(path, "r");
    if (maps == NULL) {
        return false;
    }

    while (fgets(line, sizeof(line), maps) != NULL) {
        unsigned long long start = 0;
        unsigned long long end = 0;

        if (strstr(line, module_name) == NULL) {
            continue;
        }
        if (sscanf(line, "%llx-%llx", &start, &end) != 2) {
            continue;
        }

        if (!seen) {
            low = (uint64_t)start;
            high = (uint64_t)end;
            seen = true;
        } else {
            if ((uint64_t)start < low) {
                low = (uint64_t)start;
            }
            if ((uint64_t)end > high) {
                high = (uint64_t)end;
            }
        }
    }

    fclose(maps);

    if (!seen) {
        return false;
    }

    out->pid = pid;
    out->module_base = low;
    out->module_end = high;
    snprintf(out->module_name, sizeof(out->module_name), "%s", module_name);
    return true;
}

bool mem_read(const mem_target *target, uint64_t address, void *buffer, size_t length)
{
    struct iovec local;
    struct iovec remote;
    ssize_t got;

    if (target == NULL || buffer == NULL || length == 0) {
        return false;
    }
    if (target->pid <= 0) {
        return false;
    }
    if (!mem_ptr_plausible(address)) {
        return false;
    }

    local.iov_base = buffer;
    local.iov_len = length;
    remote.iov_base = (void *)(uintptr_t)address;
    remote.iov_len = length;

    got = process_vm_readv(target->pid, &local, 1, &remote, 1, 0);

    /* Lettura parziale: fallimento. Non si pubblica mezzo dato. */
    return got == (ssize_t)length;
}

bool mem_read_u8(const mem_target *target, uint64_t address, uint8_t *out)
{
    if (out == NULL) {
        return false;
    }
    return mem_read(target, address, out, sizeof(*out));
}

bool mem_read_u32(const mem_target *target, uint64_t address, uint32_t *out)
{
    if (out == NULL) {
        return false;
    }
    return mem_read(target, address, out, sizeof(*out));
}

bool mem_read_i32(const mem_target *target, uint64_t address, int32_t *out)
{
    if (out == NULL) {
        return false;
    }
    return mem_read(target, address, out, sizeof(*out));
}

bool mem_read_u64(const mem_target *target, uint64_t address, uint64_t *out)
{
    if (out == NULL) {
        return false;
    }
    return mem_read(target, address, out, sizeof(*out));
}

bool mem_read_f32(const mem_target *target, uint64_t address, float *out)
{
    if (out == NULL) {
        return false;
    }
    return mem_read(target, address, out, sizeof(*out));
}

bool mem_read_f64(const mem_target *target, uint64_t address, double *out)
{
    if (out == NULL) {
        return false;
    }
    return mem_read(target, address, out, sizeof(*out));
}

bool mem_read_ptr(const mem_target *target, uint64_t address, uint64_t *out)
{
    uint64_t value = 0;

    if (out == NULL) {
        return false;
    }

    *out = 0;

    if (!mem_read_u64(target, address, &value)) {
        return false;
    }
    if (!mem_ptr_plausible(value)) {
        return false;
    }

    *out = value;
    return true;
}

bool mem_ptr_plausible(uint64_t address)
{
    if (address < OJBS_USER_MIN) {
        return false;
    }
    if (address >= OJBS_USER_MAX) {
        return false;
    }
    if ((address & 0x3ULL) != 0) {
        return false;
    }
    return true;
}

bool mem_in_module(const mem_target *target, uint64_t address)
{
    if (target == NULL) {
        return false;
    }
    if (target->module_base == 0 || target->module_end <= target->module_base) {
        return false;
    }
    return address >= target->module_base && address < target->module_end;
}

bool mem_looks_like_uobject(const mem_target *target, uint64_t address)
{
    uint64_t vtable = 0;

    if (!mem_ptr_plausible(address)) {
        return false;
    }
    if (!mem_read_u64(target, address, &vtable)) {
        return false;
    }
    if (!mem_ptr_plausible(vtable)) {
        return false;
    }
    if (target == NULL || target->module_base == 0) {
        return true;
    }
    return mem_in_module(target, vtable);
}
