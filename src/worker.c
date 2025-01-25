#include "worker.h"
#include "ipc_utils.h"
#include "structures.h"
#include <signal.h>

int sem_fan_count;
int sem_stand[MAX_STANDS];

void cleanup(SharedData *data, int msgid, int shmid, int msgid_manager, int sem_fan_count)
{
    printf(WORKER "[TECH_WORKER(Main)] Czyszczenie zasobów..." RESET "\n");

    // Odłączenie pamięci dzielonej
    detach_shared_memory(data);

    // Usunięcie pamięci dzielonej
    remove_shared_memory(shmid);

    // Kolejka kibiców
    remove_message_queue(msgid);

    // Kolejka komunikatów z pracownika z kierownikiem
    remove_message_queue(msgid_manager);

    // Semafor liczby kibiców na stadionie
    remove_semaphore(sem_fan_count);

    // Semafory stanowisk kontorli
    for (int i = 0; i < MAX_STANDS; i++)
    {
        remove_semaphore(sem_stand[i]);
    }
    printf(WORKER "[TECH_WORKER(Main)] Zasoby wyczyszczone" RESET "\n");
}

void handle_signal(int signal)
{   
    // Klucze IPC
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_sem_fan = get_key(KEY_PATH, SEM_FAN_COUNT_ID);

    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0);
    
    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Semafor liczby kibiców na stadionie
    sem_fan_count = get_semaphore(key_sem_fan, 0);

    // Zablokowanie semafora
    semaphore_wait(sem_fan_count);

    // Wybór działania w zależności od sygnału
    switch (signal)
    {
    case SIGUSR1: // Wstrzymanie wpuszczania kibiców
        data->entry_paused = true;
        printf(WORKER "[TECH_WORKER(Main)] Wstrzymano wpuszczanie kibiców\n" RESET);
        break;
    case SIGUSR2: // Wznowienie wpuszczania kibiców
        data->entry_paused = false;
        printf(WORKER "[TECH_WORKER(Main)] Wznowiono wpuszczanie kibiców\n" RESET);
        break;
    case SIGTERM: // Rozpoczęcie ewakuacji
        data->evacuation = true;
        printf(WORKER "[TECH_WORKER(Main)] Rozpoczęto ewakuację\n" RESET);
        break;
    }

    // Zwolnienie semafora
    semaphore_signal(sem_fan_count);

    // Odłączenie pamięci dzielonej
    detach_shared_memory(data);
}

void evacuate_stadium(SharedData *data, int msgid_manager)
{
    // Klucze IPC
    key_t key_sem_fan = get_key(KEY_PATH, SEM_FAN_COUNT_ID);
    
    // Semafor liczby kibiców na stadionie
    int sem_fan_count = get_semaphore(key_sem_fan, 0);

    // Przeprowadzamy ewakuację do momentu opuszczenia stadionu przez ostatniego kibica
    while (1)
    {
        // Zablokowanie semafora
        semaphore_wait(sem_fan_count);
        sleep(1);
        if (data->fans_in_stadium > 0) // Jeśli są jacyś kibice na stadionie...
        {
            data->fans_in_stadium--; // ...zmniejsz liczbę kibiców o jeden...
            printf(WORKER "[TECH_WORKER(Main)] Ewakuacja: pozostało %d kibiców\n" RESET, data->fans_in_stadium); // ...wyświetl liczbę kibiców na stadionie...
            semaphore_signal(sem_fan_count); // ...zwolnij semafor
        }
        else // Jeśli wszyscy kibice zostali ewakuowani...
        {
            data->evacuation = false; // ...zakończ ewakuację...
            semaphore_signal(sem_fan_count); // ...i zwolnij semafor
            break;
        }
    }

    // Wysłanie wiadomości do kierownika
    QueueMessage msg;
    msg.message_type = EVACUATION_COMPLETE;
    msg.sender = getpid();

    if (msgsnd(msgid_manager, &msg, sizeof(QueueMessage) - sizeof(long), 0) == -1)
    {
        perror(ERROR "Błąd wysłania komunikatu EVACUATION_COMPLETE");
        exit(1);
    }
    printf(WORKER "[TECH_WORKER(Main)] Ewakuacja zakończona\n" RESET);
}

void run_worker_main()
{
    // Klucze IPC
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_msg = get_key(KEY_PATH, MSG_ID);
    key_t key_msg_manager = get_key(KEY_PATH, MSG_MANAGER_ID);
    key_t key_sem_fan = get_key(KEY_PATH, SEM_FAN_COUNT_ID);

    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), IPC_CREAT | 0666);

    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Inicjalizacja kolejki kibiców oraz kolejki komunikatów do kierownika
    int msgid = create_message_queue(key_msg, IPC_CREAT | 0666);
    int msgid_manager = create_message_queue(key_msg_manager, IPC_CREAT | 0666);

    // Inicjalizacja semafora liczby kibiców na stadionie
    int sem_fan_count = create_semaphore(key_sem_fan, 1, IPC_CREAT | 0666);

    // Inicjalizacja semaforów stanowisk kontroli
    for (int i = 0; i < MAX_STANDS; i++)
    {
        // Klucz semafora
        key_t key_sem_stand = get_key(KEY_PATH, SEM_STAND_BASE + i);
        // Inicjalizacja semafora
        sem_stand[i] = create_semaphore(key_sem_stand, MAX_PEOPLE_PER_STAND, IPC_CREAT | 0666);

        // Ustawienie liczby kibiców w stanowisku kontroli
        data->fans_on_stand[i] = 0;
    }

    // Ustawienie liczby kibiców na stadionie oraz flag wstrzymywania
    // wpuszczania kibiców i ewakuacji
    data->fans_in_stadium = 0;
    data->entry_paused = false;
    data->evacuation = false;

    printf(WORKER "[TECH_WORKER(Main)] Główny pracownik techniczny uruchomiony" RESET "\n");

    // Wstrzymanie wpuszczania kibiców
    signal(SIGUSR1, handle_signal);
    // Wznowienie wpuszczania kibiców
    signal(SIGUSR2, handle_signal);
    // Ewakuacja
    signal(SIGTERM, handle_signal);

    // Czekanie na sygnały lub maksymalną liczbę kibiców
    while (1)
    {
        // Zablokowanie semafora liczby kibiców
        semaphore_wait(sem_fan_count);

        // Jeśli flaga ewakuacji jest ustawiona na true...
        if (data->evacuation)
        {
            // Zwolnij semafor liczby kibiców
            semaphore_signal(sem_fan_count);
            // Przeprowadź ewakucję
            evacuate_stadium(data, msgid_manager);
            break;
        }

        // Jeśli liczba kibiców osiągnęła maksymalną liczbę, nie ma ewakuacji
        // oraz wstrzymania wpuszczania kibiców
        if (data->fans_in_stadium >= MAX_FANS && data->evacuation == false && data->entry_paused == false)
        {
            // Sprawdzamy czy wszystkie stanowiska kontroli skończyły pracę
            bool all_stands_empty = true;
            for (int i = 0; i < MAX_STANDS; i++)
            {   
                // Jeśli nie to przerywamy kończenie pracy pracownika technicznego(main)
                if (data->fans_on_stand[i] > 0)
                {
                    all_stands_empty = false;
                    break;
                }
            }
            // W przeciwnym wypadku...
            if (all_stands_empty)
            {
                semaphore_signal(sem_fan_count); // ...zwalniamy semafor liczby kibiców...
                printf(WORKER "[TECH_WORKER(Main)] Stadion pełny i wszystkie stanowiska są puste. Kończymy pracę." RESET "\n");
                break; //...i kończymy pracę pracownika technicznego(main)
            }
        }
        // Zwalniamy semafor jeśli pracownik techniczny dalej pracuje
        semaphore_signal(sem_fan_count);
        sleep(1);
    }

    // Usuwanie mechanizmów IPC
    cleanup(data, msgid, shmid, msgid_manager, sem_fan_count);
    printf(WORKER "[TECH_WORKER] Główny pracownik techniczny zakończył działanie" RESET "\n");
    exit(0);
}

void run_worker_stand(int stand_id)
{   
    // Klucze IPC
    key_t key_msg = get_key(KEY_PATH, MSG_ID);
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_sem_stand = get_key(KEY_PATH, SEM_STAND_BASE + stand_id);

    // Kolejka kibiców
    int msgid = create_message_queue(key_msg, 0);
    
    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0);

    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Semafor stanowiska kontroli
    int control_sem = get_semaphore(key_sem_stand, 0);

    printf(WORKER "[TECH_WORKER] Pracownik stanowiska %d uruchomiony" RESET "\n", stand_id + 1);

    FanData fan;
    while (1)
    {
        // Blokowanie stanowiska
        semaphore_wait(control_sem);

        // Sprawdzenie czy kontrola jest wstrzymana
        if (data->entry_paused)
        {
            printf(WORKER "[TECH_WORKER] Stanowisko %d wstrzymane\n" RESET, stand_id);
            semaphore_signal(control_sem);
            sleep(1);
            continue;
        }

        // Sprawdzenie czy jest ewakuacja
        if (data->evacuation)
        {
            printf(WORKER "[TECH_WORKER] Stanowisko %d kończy pracę z powodu ewakuacji" RESET "\n", stand_id);
            semaphore_signal(control_sem);
            break;
        }

        // Sprawdzenie czy osiągnięto limit kibiców
        if (data->fans_in_stadium >= MAX_FANS)
        {
            printf(WORKER "[TECH_WORKER] Stanowisko %d kończy pracę: stadion pełny" RESET "\n", stand_id);
            semaphore_signal(control_sem);
            break;
        }

        // Odebranie wiadomości od kibica w kolejce
        if (msgrcv(msgid, &fan, sizeof(FanData) - sizeof(long), 1, IPC_NOWAIT) == -1)
        {

            // Odblokuj stanowisko, jeśli brak kibiców w kolejce
            semaphore_signal(control_sem);
            if (data->fans_in_stadium >= MAX_FANS)
            {
                printf(WORKER "[TECH_WORKER] Stanowisko %d kończy pracę: brak kibiców w kolejce" RESET "\n", stand_id);
                break;
            }
            continue;
        }
        else
        {
            // Symulacja kontroli
            printf(WORKER "[TECH_WORKER] Stanowisko %d obsługuje kibica: ID=%d, Wiek=%d, Team=%d, VIP=%d, Dziecko=%d" RESET "\n",
                   stand_id, fan.fan_id, fan.age, fan.team, fan.is_vip, fan.is_child);

            sleep(1);

            // Zwiększ lcizbę kibiców na stadionie
            data->fans_in_stadium++;

            printf(WORKER "[TECH_WORKER] Stanowisko %d - Kibic: ID=%d, Team=%d, VIP=%d, Child=%d, Age=%d skontrolowany i wpuszczony na stadion" RESET "\n",
                   stand_id, fan.fan_id, fan.team, fan.is_vip, fan.is_child, fan.age);

            // Odblokuj stanowisko
            semaphore_signal(control_sem);
        }
    }

    // Odłączenie pamięci dzielonej
    detach_shared_memory(data);
    exit(0);
}