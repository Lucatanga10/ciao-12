#ifndef OJBS_MEMREAD_H
#define OJBS_MEMREAD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/*
 * Lettura sola. Nessuna scrittura, nessun ptrace, nessun attach: solo
 * process_vm_readv, che e' una syscall e lascia il processo target intatto.
 * Una lettura parziale e' un fallimento, non uno zero.
 */

typedef struct {
    pid_t pid;
    uint64_t module_base;
    uint64_t module_end;
    char module_name[128];
} mem_target;

/* Trova il pid dal nome pacchetto leggendo /proc/<pid>/cmdline. 0 se assente. */
pid_t mem_find_pid(const char *package);

/* Riempie base ed end dal primo e dall'ultimo mapping che porta quel nome. */
bool mem_find_module(pid_t pid, const char *module_name, mem_target *out);

bool mem_read(const mem_target *target, uint64_t address, void *buffer, size_t length);

bool mem_read_u8(const mem_target *target, uint64_t address, uint8_t *out);
bool mem_read_u32(const mem_target *target, uint64_t address, uint32_t *out);
bool mem_read_i32(const mem_target *target, uint64_t address, int32_t *out);
bool mem_read_u64(const mem_target *target, uint64_t address, uint64_t *out);
bool mem_read_f32(const mem_target *target, uint64_t address, float *out);
bool mem_read_f64(const mem_target *target, uint64_t address, double *out);

/* Puntatore letto e gia' validato: 0 scritto in out quando non e' plausibile. */
bool mem_read_ptr(const mem_target *target, uint64_t address, uint64_t *out);

/* Spazio utente arm64, allineato a 4. Non prova che l'oggetto esista. */
bool mem_ptr_plausible(uint64_t address);

bool mem_in_module(const mem_target *target, uint64_t address);

/*
 * Sostituto del check klass di IL2CPP: ogni UObject ha una vtable a +0x0 che
 * punta dentro il modulo. Se il modulo non e' noto, il check si degrada a
 * sola plausibilita'.
 */
bool mem_looks_like_uobject(const mem_target *target, uint64_t address);

#endif /* OJBS_MEMREAD_H */
