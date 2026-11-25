#ifndef OJBS_SELFTEST_H
#define OJBS_SELFTEST_H

/*
 * Verifica le parti pure: matematica e serializzazione. Non tocca il processo
 * target, non apre socket, non serve root. Gira anche sull'host.
 * Torna il numero di test falliti: 0 significa tutto verde.
 */
int selftest_run(void);

#endif /* OJBS_SELFTEST_H */
