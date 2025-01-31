#include "worker.h"
#include "ipc_utils.h"
#include "structures.h"
#include <signal.h>

void cleanup(SharedData *data, int msgid, int shmid, int msgid_manager, int sem_fan_count, int sem_main_tech, int *sem_stand, int sem_fans_waiting)
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

    remove_semaphore(sem_main_tech);

    // Semafor liczby kibiców w kolejce
    remove_semaphore(sem_fans_waiting);

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
    key_t key_sem_tech = get_key(KEY_PATH, SEM_MAIN_TECH_ID);

    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0666);

    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Semafor liczby kibiców na stadionie
    int sem_main_tech = get_semaphore(key_sem_tech, 0666);

    // Zablokowanie semafora
    semaphore_wait(sem_main_tech);

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
    semaphore_signal(sem_main_tech);

    // Odłączenie pamięci dzielonej
    // detach_shared_memory(data);
}

void evacuate_stadium(SharedData *data, int msgid_manager)
{
    // Klucze IPC
    key_t key_sem_tech = get_key(KEY_PATH, SEM_MAIN_TECH_ID);

    // Semafor liczby kibiców na stadionie
    int sem_main_tech = get_semaphore(key_sem_tech, 0666);

    // Przeprowadzamy ewakuację do momentu opuszczenia stadionu przez ostatniego kibica
    while (1)
    {
        // Zablokowanie semafora
        semaphore_wait(sem_main_tech);
        sleep(1);
        if (data->fans_in_stadium > 0) // Jeśli są jacyś kibice na stadionie...
        {
            data->fans_in_stadium--;                                                                             // ...zmniejsz liczbę kibiców o jeden...
            printf(WORKER "[TECH_WORKER(Main)] Ewakuacja: pozostało %d kibiców\n" RESET, data->fans_in_stadium); // ...wyświetl liczbę kibiców na stadionie...
            semaphore_signal(sem_main_tech);                                                                     // ...zwolnij semafor
        }
        else // Jeśli wszyscy kibice zostali ewakuowani...
        {
            data->evacuation = false;        // ...zakończ ewakuację...
            semaphore_signal(sem_main_tech); // ...i zwolnij semafor
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
    key_t key_sem_tech = get_key(KEY_PATH, SEM_MAIN_TECH_ID);
    key_t key_fan_count = get_key(KEY_PATH, SEM_FAN_COUNT_ID);
    key_t key_msg_manager = get_key(KEY_PATH, MSG_MANAGER_ID);

    key_t key_sem_waiting_fans = get_key(KEY_PATH, SEM_WAITING_FANS);

    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), IPC_CREAT | IPC_EXCL | 0666);

    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Inicjalizacja kolejki kibiców oraz kolejki komunikatów do kierownika
    int msgid = create_message_queue(key_msg, IPC_CREAT | IPC_EXCL | 0666);
    int msgid_manager = create_message_queue(key_msg_manager, IPC_CREAT | IPC_EXCL | 0666);

    // Inicjalizacja semafora liczby kibiców na stadionie
    int sem_fan_count = create_semaphore(key_fan_count, 1, IPC_CREAT | IPC_EXCL | 0666);

    int sem_main_tech = create_semaphore(key_sem_tech, 1, IPC_CREAT | IPC_EXCL | 0666);

    // Inicjalizacja semafora liczby kibiców w kolejce
    int sem_fans_waiting = create_semaphore(key_sem_waiting_fans, 0, IPC_CREAT | IPC_EXCL | 0666);

    // Inicjalizacja semaforów stanowisk kontroli
    int sem_stand[MAX_STANDS];
    for (int i = 0; i < MAX_STANDS; i++)
    {
        // Klucz semafora
        key_t key_sem_stand = get_key(KEY_PATH, SEM_STAND_BASE + i);
        // Inicjalizacja semafora
        sem_stand[i] = create_semaphore(key_sem_stand, MAX_PEOPLE_PER_STAND, IPC_CREAT | IPC_EXCL | 0666);

        // Ustawienie liczby kibiców w stanowisku kontroli
        data->fans_on_stand[i] = 0;
        data->fans_on_stand[i] = 0;
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
        semaphore_wait(sem_main_tech);

        // Jeśli flaga ewakuacji jest ustawiona na true...
        if (data->evacuation)
        {
            // Zwolnij semafor liczby kibiców
            semaphore_signal(sem_main_tech);
            // Przeprowadź ewakucję
            evacuate_stadium(data, msgid_manager);
            break;
        }

        printf(WORKER "Liczba kibiców na stanowiskach[1]-%d,[2]-%d,[3]-%d\n" RESET, data->fans_on_stand[0], data->fans_on_stand[1], data->fans_on_stand[2]);
        // Jeśli liczba kibiców osiągnęła maksymalną liczbę, nie ma ewakuacji
        // oraz wstrzymania wpuszczania kibiców
        if (data->fans_in_stadium >= MAX_FANS && data->evacuation == false && data->entry_paused == false)
        {
            // Sprawdzamy czy wszystkie stanowiska kontroli skończyły pracę
            bool all_stands_empty = true;
            for (int i = 0; i < MAX_STANDS; i++)
            {
                // Jeśli nie to przerywamy kończenie pracy pracownika technicznego(main)
                if (data->fans_on_stand[i] > 0 || data->fans_on_stand[i] > 0 || data->fans_on_stand[i] > 0)
                {
                    all_stands_empty = false;
                    break;
                }
            }
            // W przeciwnym wypadku...
            if (all_stands_empty)
            {
                semaphore_signal(sem_main_tech); // ...zwalniamy semafor liczby kibiców...
                printf(WORKER "[TECH_WORKER(Main)] Stadion pełny i wszystkie stanowiska są puste. Kończymy pracę." RESET "\n");
                break; //...i kończymy pracę pracownika technicznego(main)
            }
        }
        // Zwalniamy semafor jeśli pracownik techniczny dalej pracuje
        semaphore_signal(sem_main_tech);
        sleep(2);
    }

    // Usuwanie mechanizmów IPC
    cleanup(data, msgid, shmid, msgid_manager, sem_fan_count, sem_main_tech, sem_stand, sem_fans_waiting);
    printf(WORKER "[TECH_WORKER] Główny pracownik techniczny zakończył działanie" RESET "\n");
    exit(0);
}

void run_worker_stand(int stand_id)
{
    // Klucze IPC
    key_t key_msg = get_key(KEY_PATH, MSG_ID);
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_sem_stand = get_key(KEY_PATH, SEM_STAND_BASE + stand_id);
    key_t key_sem_fans_waiting = get_key(KEY_PATH, SEM_WAITING_FANS);

    // Kolejka kibiców
    int msgid = create_message_queue(key_msg, 0666);

    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0666);

    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Semafor stanowiska kontroli
    int control_sem = get_semaphore(key_sem_stand, 0666);

    // Semafor liczby kibiców w kolejce
    int sem_fans_waiting = get_semaphore(key_sem_fans_waiting, 0666);

    // Tablica komunikatów kibica
    int message_types[] = {JOIN_CONTROL, LET_FAN_GO, AGGRESSIVE_FAN};

    // Liczba komunikatów
    size_t message_count = sizeof(message_types) / sizeof(message_types[0]);

    // Flaga odebranej wiadomości
    int message_receive = 0;

    printf(WORKER "[TECH_WORKER] Pracownik stanowiska %d uruchomiony" RESET "\n", stand_id + 1);

    // Kibice na stanowisku kontroli
    FanData stand_fans[MAX_PEOPLE_PER_STAND];

    for (int i = 0; i < MAX_PEOPLE_PER_STAND; i++)
    {
        reset_fan_data(&stand_fans[i]);
    }

    // Ilość kibiców do skontrolowania (zmniejszany, gdy kibic jest z innej drużyny)
    int stand_fan_id = 0;

    // Licznik zaproszonych kibiców
    int invite_counter = 0;

    bool same_team = true;

    while (1)
    {
        // Odebrana wiadomość
        QueueMessage message;

        // Sprawdzenie czy kontrola jest wstrzymana
        if (data->entry_paused)
        {
            printf(WORKER "[TECH_WORKER] Stanowisko %d wstrzymane\n" RESET, stand_id + 1);
            // semaphore_signal(control_sem);
            sleep(1);
            continue;
        }

        // Sprawdzenie czy jest ewakuacja
        if (data->evacuation)
        {
            printf(WORKER "[TECH_WORKER] Stanowisko %d kończy pracę z powodu ewakuacji" RESET "\n", stand_id + 1);
            // semaphore_signal(control_sem);
            break;
        }

        // Sprawdzenie czy osiągnięto limit kibiców
        if (data->fans_in_stadium >= MAX_FANS)
        {
            printf(WORKER "[TECH_WORKER] Stanowisko %d kończy pracę: stadion pełny" RESET "\n", stand_id + 1);
            // semaphore_signal(control_sem);
            break;
        }

        // Odebranie wiadomości od kibica w kolejce
        if (msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), JOIN_CONTROL, IPC_NOWAIT) == -1)
        {
            if (msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), LET_FAN_GO, IPC_NOWAIT) == -1)
            {
                if (msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), AGGRESSIVE_FAN, IPC_NOWAIT) == -1)
                {
                    if (msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), VIP_ENTER, IPC_NOWAIT) == -1)
                    {
                        // semaphore_signal(control_sem);
                        continue;
                    }
                }
            }
        }

        // Blokowanie stanowiska
        semaphore_wait(control_sem);

        // Zapisujemy dane kibica, który chce wejść do kontroli
        stand_fans[stand_fan_id] = message.fData;

        // Sprawdzamy drużynę kibica jeśli nie jest pierwszym kibicem zaproszonym do kontroli
        if (stand_fan_id > 0)
        {
            same_team = check_fans_team(stand_fans[stand_fan_id].team, stand_fans, stand_fan_id);
        }

        // Jeśli liczba kibiców do kontroli jest mniejsza od 3 oraz kibic jest 
        // z tej samej drużyny oraz nie jest VIP
        if (stand_fan_id < MAX_PEOPLE_PER_STAND && same_team && message.message_type != VIP_ENTER)
        {
            // Zwiększamy liczbę kibiców na stanowisku
            (data->fans_on_stand[stand_id])++;

            // Zwiększamy liczbę kibiców przystępujących do kontroli
            stand_fan_id++;
            
            if (data->fans_on_stand[stand_id] < MAX_PEOPLE_PER_STAND)
            {
                if (get_sem_value(sem_fans_waiting) > 0)
                {
                    semaphore_signal(control_sem);
                    continue;
                }
            }
        }
        // Jeśli kibic jest przeciwnej drużyny i nie jest VIP
        if (!same_team && message.message_type != VIP_ENTER)
        {
            message.message_type = OTHER_TEAM;
            message.sender = getpid();
            message.fData = stand_fans[stand_fan_id];
            
            // Wysyłamy wiadomość o tym że musi przepuścić kibica
            if (msgsnd(msgid, &message, sizeof(QueueMessage) - sizeof(long), 0) == -1)
            {
                perror(ERROR "Błąd wysłania wiadomości OTHER_TEAM do kibica" RESET);
                semaphore_signal(control_sem);
            }
            stand_fan_id--;
            // reset_fan_data(&stand_fans[stand_fan_id]);
        }
        if (message.message_type == VIP_ENTER)
        {
            printf(WORKER "[TECH_WORKER] Kibic %d(VIP) - Wiek=%d, Team=%d, VIP=%d, Dziecko=%d, Poziom agresji=%d wchodzi bez kolejki\n" RESET, stand_fans[stand_fan_id].fan_id, stand_fans[stand_fan_id].age, stand_fans[stand_fan_id].team, stand_fans[stand_fan_id].is_vip, stand_fans[stand_fan_id].is_child, stand_fans[stand_fan_id].aggressive_counter);

            (data->fans_in_stadium) += 1;

            send_controlled_fan_message(data, &msgid, stand_fans, message, &stand_fan_id);

            stand_fan_id--;

            semaphore_signal(control_sem);
            continue;
        }
        if (stand_fan_id > 0)
        {
            control_fans(stand_fans, stand_id, &stand_fan_id);

            // Zwiększ liczbę kibiców na stadionie
            (data->fans_in_stadium) += stand_fan_id;
            data->fans_on_stand[stand_id] = 0;

            send_controlled_fan_message(data, &msgid, stand_fans, message, &stand_fan_id);

            stand_fan_id = 0;
            invite_counter = 0;
        }

        // Odblokuj stanowisko
        semaphore_signal(control_sem);
        log_file(data);
    }

    // Odłączenie pamięci dzielonej
    detach_shared_memory(data);
    exit(0);
}

void control_fans(FanData *fans, int stand_id, int *stand_fan_id)
{
    // Liczba kontrolowanych kibiców
    printf(WORKER "[TECH_WORKER] Stanowisko %d kontroluje %d kibiców: \n" RESET, stand_id + 1, *stand_fan_id);
    
    // Kontrola kibiców na stanowisku
    for (int i = 0; i < (*stand_fan_id); i++)
    {
        printf(WORKER "[TECH_WORKER] Kibic %d - Wiek=%d, Team=%d, VIP=%d, Dziecko=%d, Poziom agresji=%d\n" RESET, fans[i].fan_id, fans[i].age, fans[i].team, fans[i].is_vip, fans[i].is_child, fans[i].aggressive_counter);
    }

    printf(WORKER "[TECH_WORKER] Stanowisko %d skontrolowało %d kibiców: \n" RESET, stand_id + 1, *stand_fan_id);
}

void send_controlled_fan_message(SharedData *data, int *msgid, FanData *fans, QueueMessage message, int *stand_fan_id)
{
    // Wysłanie wiadomośći do VIPa
    if (message.message_type == VIP_ENTER)
    {
        message.message_type = fans[*stand_fan_id].fan_pid;
        message.sender = getpid();
        message.fData = fans[*stand_fan_id];
        if (msgsnd(*msgid, &message, sizeof(QueueMessage) - sizeof(long), 0) == -1)
        {
            perror(ERROR "Błąd wysłania wiadomości HAVE_FUN do kibica" RESET);
            exit(1);
        }
    }
    else
    {
        // Wysłanie wiadomości do zwykłych kibiców
        for (int i = 0; i < (*stand_fan_id); i++)
        {
            message.message_type = fans[i].fan_pid;
            message.sender = getpid();
            message.fData = fans[i];
            if (msgsnd(*msgid, &message, sizeof(QueueMessage) - sizeof(long), 0) == -1)
            {
                perror(ERROR "Błąd wysłania wiadomości HAVE_FUN do kibica" RESET);
                exit(1);
            }
        }
    }
}

void reset_fan_data(FanData *fan)
{
    fan->fan_pid = 1;
    fan->fan_id = 0;
    fan->team = TEAM_UNKNOWN;
    fan->age = 0;
    fan->is_vip = false;
    fan->is_child = false;
    fan->aggressive_counter = 0;
}

bool check_fans_team(Team checking_team, FanData *stand_fans, int fan_idx)
{
    if (fan_idx == 0)
    {
        return true;
    }
    else if (stand_fans[fan_idx - 1].team == checking_team)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void log_file(SharedData *data)
{
    FILE *logs = fopen("logi", "w");
    char text[128] = "Liczba kibiców na stadionie: ";
    if (logs == NULL)
    {
        perror(ERROR "Nie można otworzyć pliku logi\n" RESET);
        exit(1);
    }

    char buffer[32];
    sprintf(buffer, "%d", data->fans_in_stadium);
    strcat(text, buffer);
    strcat(text, "\n");

    fprintf(logs, "%s", text);

    fclose(logs);
}