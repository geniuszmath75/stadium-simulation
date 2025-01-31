#include <wait.h>
#include "ipc_utils.h"
#include "worker.h"
#include "manager.h"
#include "fan.h"
#include "structures.h"
#include <signal.h>

int main()
{
    // PIDy pracowników technicznych, kierownika oraz kibiców
    pid_t worker_main_pid, worker_stand_pid[MAX_STANDS], manager_pid, fan_pid;
    // Liczba kibiców którzy chcą wejść na stadion
    int number_of_fans = 0;
    char input[10];

    // Dopóki wprowadzona liczba fanów będzie mniejsza od 0...
    while (number_of_fans < 1)
    {
        printf("Podaj liczbę kibiców chcących wejść na stadion: ");
        scanf("%s", input); //...podajemy liczbę kibiców...

        //...sprawdzamy, czy wprowadzona wartość jest liczbą...
        char *endptr;
        number_of_fans = strtol(input, &endptr, 10); // Zamiana chara* na liczbę
        if (*endptr != '\0' || number_of_fans < 1) //...i jeśli nie jest...
        {
            printf(ERROR "Liczba musi być większa od 0\n" RESET);
            number_of_fans = 0; //...to resetujemy wartości, aby ponownie zapytać użytkownika
        }
    }
    

    // Uruchomienie głównego pracownika technicznego
    if ((worker_main_pid = fork()) == 0)
    {
        printf(WORKER "[TECH_WORKER(Main)] Uruchamianie głównego pracownika technicznego" RESET "\n");
        run_worker_main();
        exit(0);
    }

    // Uruchomienie pracowników stanowisk
    for (int i = 0; i < MAX_STANDS; i++)
    {
        if ((worker_stand_pid[i] = fork()) == 0)
        {
            printf(WORKER "[TECH_WORKER] Uruchamianie pracownika stanowiska %d" RESET "\n", i + 1);
            run_worker_stand(i);
            exit(0);
        }
    }

    // Uruchomienie kierownika
    if ((manager_pid = fork()) == 0)
    {
        printf(MANAGER "[MANAGER] Uruchamianie kierownika \n" RESET);
        run_manager(worker_main_pid);
        exit(0);
    }

    // Uruchomienie kibiców
    for (int i = 0; i < number_of_fans; i++)
    { // Liczba kibiców
        if ((fan_pid = fork()) == 0)
        {
            run_fan(i+1);
            exit(0);
        }
        sleep(1);
    }

    // Czekanie na zakończenie procesów
    waitpid(worker_main_pid, NULL, 0);
    for (int i = 0; i < MAX_STANDS; i++)
    {
        waitpid(worker_stand_pid[i], NULL, 0);
    }
    waitpid(manager_pid, NULL, 0);
    for (int i = 0; i < number_of_fans; i++)
    {
        wait(NULL);
    }

    return 0;
}