#include <wait.h>
#include "ipc_utils.h"
#include "worker.h"
#include "manager.h"
#include "fan.h"
#include "structures.h"

int main() {
    pid_t worker_main_pid, worker_stand_pid[MAX_STANDS], manager_pid, fan_pid;

    // Uruchomienie głównego pracownika technicznego
    if ((worker_main_pid = fork()) == 0) {
        printf(WORKER "[TECH_WORKER] Uruchamianie głównego pracownika technicznego" RESET "\n");
        run_worker_main();
        exit(0);
    }

    // Uruchomienie pracowników stanowisk
    for (int i = 0; i < MAX_STANDS; i++) {
        if ((worker_stand_pid[i] = fork()) == 0) {
            printf(WORKER "[TECH_WORKER] Uruchamianie pracownika stanowiska %d" RESET "\n", i);
            run_worker_stand(i);
            exit(0);
        }
    }

    // Uruchomienie kierownika
    if ((manager_pid = fork()) == 0) {
        printf(MANAGER "[MANAGER] Uruchamianie kierownika" RESET "\n");
        run_manager();
        exit(0);
    }

    // Uruchomienie kibiców
    for (int i = 0; i < MAX_FANS; i++) { // Liczba kibiców
        if ((fan_pid = fork()) == 0) {
            printf(FAN "[FAN] Uruchamianie kibica %d" RESET "\n", i);
            run_fan(i);
            exit(0);
        }
        sleep(1);
    }

    // Czekaj na zakończenie procesów
    waitpid(worker_main_pid, NULL, 0);
    for (int i = 0; i < MAX_STANDS; i++) {
        waitpid(worker_stand_pid[i], NULL, 0);
    }
    waitpid(manager_pid, NULL, 0);
    for (int i = 0; i < MAX_FANS; i++) {
        wait(NULL);
    }

    return 0;
}