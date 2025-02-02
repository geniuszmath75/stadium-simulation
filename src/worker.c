#include "worker.h"
#include "ipc_utils.h"
#include "structures.h"
#include <signal.h>

void cleanup(SharedData *data, int msgid, int shmid, int msgid_manager, int sem_fan_count, int sem_evacuation, int sem_entry_paused, int sem_stands, int sem_fans_waiting)
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

    // Semafor flagi ewakuacji
    remove_semaphore(sem_evacuation);

    // Semafor flagi wstrzymania kontroli
    remove_semaphore(sem_entry_paused);

    // Semafor liczby kibiców w kolejce
    remove_semaphore(sem_fans_waiting);

    // Semafory stanowisk kontorli
    remove_semaphore(sem_stands);
    printf(WORKER "[TECH_WORKER(Main)] Zasoby wyczyszczone" RESET "\n");
}

void handle_signal(int signal)
{
    // Klucze IPC
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_evacuation = get_key(KEY_PATH, SEM_EVACUATION_ID);
    key_t key_entry_paused = get_key(KEY_PATH, SEM_ENTRY_PAUSED_ID);

    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0666);

    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Semafor flagi ewakuacji
    int sem_evacuation = get_semaphore(key_evacuation, 0666);

    // Semafor flagi wstrzymania kontroli
    int sem_entry_paused = get_semaphore(key_entry_paused, 0666);

    // Wybór działania w zależności od sygnału
    switch (signal)
    {
    case SIGUSR1: // Wstrzymanie wpuszczania kibiców
        semaphore_wait(sem_entry_paused, 0);
        data->entry_paused = true;
        semaphore_signal(sem_entry_paused, 0);
        printf(WORKER "[TECH_WORKER(Main)] Wstrzymano wpuszczanie kibiców\n" RESET);
        break;
    case SIGUSR2: // Wznowienie wpuszczania kibiców
        semaphore_wait(sem_entry_paused, 0);
        data->entry_paused = false;
        semaphore_signal(sem_entry_paused, 0);
        printf(WORKER "[TECH_WORKER(Main)] Wznowiono wpuszczanie kibiców\n" RESET);
        break;
    case SIGTERM: // Rozpoczęcie ewakuacji
        semaphore_signal(sem_evacuation, 0);
        data->evacuation = true;
        semaphore_signal(sem_evacuation, 0);
        printf(WORKER "[TECH_WORKER(Main)] Rozpoczęto ewakuację\n" RESET);
        break;
    }
}

void evacuate_stadium(SharedData *data, int msgid_manager)
{
    // Klucze IPC
    key_t key_fan_count = get_key(KEY_PATH, SEM_FAN_COUNT_ID);
    key_t key_evacuation = get_key(KEY_PATH, SEM_EVACUATION_ID);

    // Semafor liczby kibiców na stadionie
    int sem_fan_count = get_semaphore(key_fan_count, 0666);

    // Semafor flagi ewakuacji
    int sem_evacuation = get_semaphore(key_evacuation, 0666);

    // Przeprowadzamy ewakuację do momentu opuszczenia stadionu przez ostatniego kibica
    while (1)
    {
        // Zablokowanie semafora
        sleep(1);
        if (data->fans_in_stadium > 0) // Jeśli są jacyś kibice na stadionie...
        {
            semaphore_wait(sem_fan_count, 0);
            data->fans_in_stadium--;                                                                             // ...zmniejsz liczbę kibiców o jeden...
            printf(WORKER "[TECH_WORKER(Main)] Ewakuacja: pozostało %d kibiców\n" RESET, data->fans_in_stadium); // ...wyświetl liczbę kibiców na stadionie...
            semaphore_signal(sem_fan_count, 0);                                                                  // ...zwolnij semafor
        }
        else // Jeśli wszyscy kibice zostali ewakuowani...
        {
            // semaphore_wait(sem_evacuation, 0);
            // data->evacuation = false;        // ...zakończ ewakuację...
            // semaphore_signal(sem_evacuation, 0); // ...i zwolnij semafor
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

    if (msgrcv(msgid_manager, &msg, sizeof(QueueMessage) - sizeof(long), FINISH_WORKER, 0) != -1)
    {
        return;
    }
}

void run_worker_main()
{
    // Klucze IPC
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_msg = get_key(KEY_PATH, MSG_ID);
    key_t key_evacuation = get_key(KEY_PATH, SEM_EVACUATION_ID);
    key_t key_fan_count = get_key(KEY_PATH, SEM_FAN_COUNT_ID);
    key_t key_entry_paused = get_key(KEY_PATH, SEM_ENTRY_PAUSED_ID);
    key_t key_sem_waiting_fans = get_key(KEY_PATH, SEM_WAITING_FANS);
    key_t key_msg_manager = get_key(KEY_PATH, MSG_MANAGER_ID);
    key_t key_sem_stand = get_key(KEY_PATH, SEM_STAND_BASE);

    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), IPC_CREAT | IPC_EXCL | 0666);

    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Inicjalizacja kolejki kibiców oraz kolejki komunikatów do kierownika
    int msgid = create_message_queue(key_msg, IPC_CREAT | IPC_EXCL | 0666);
    int msgid_manager = create_message_queue(key_msg_manager, IPC_CREAT | IPC_EXCL | 0666);

    // Inicjalizacja semafora liczby kibiców na stadionie
    int sem_fan_count = create_semaphore(key_fan_count, 1, 1, IPC_CREAT | IPC_EXCL | 0666);

    // Semafor flagi ewakuacji
    int sem_evacuation = create_semaphore(key_evacuation, 1, 1, IPC_CREAT | IPC_EXCL | 0666);

    // Semafor flagi wstrzymania kontroli
    int sem_entry_paused = create_semaphore(key_entry_paused, 1, 1, IPC_CREAT | IPC_EXCL | 0666);

    // Inicjalizacja semafora liczby kibiców w kolejce
    int sem_fans_waiting = create_semaphore(key_sem_waiting_fans, 1, MAX_FANS, IPC_CREAT | IPC_EXCL | 0666);

    // Inicjalizacja semaforów stanowisk kontroli
    int sem_stands = create_semaphore(key_sem_stand, 3, 1, IPC_CREAT | IPC_EXCL | 0666);
    for (int i = 1; i <= MAX_STANDS; i++)
    {
        // Ustawienie liczby kibiców w stanowisku kontroli
        semaphore_wait(sem_stands, i - 1);
        data->fans_on_stand[i - 1] = 0;
        data->fans_on_stand[i - 1] = 0;
        data->fans_on_stand[i - 1] = 0;
        semaphore_signal(sem_stands, i - 1);
    }

    // Ustawienie liczby kibiców na stadionie oraz flag wstrzymywania
    // wpuszczania kibiców i ewakuacji
    semaphore_wait(sem_fan_count, 0);
    data->fans_in_stadium = 0;
    semaphore_signal(sem_fan_count, 0);

    semaphore_wait(sem_entry_paused, 0);
    data->entry_paused = false;
    semaphore_signal(sem_entry_paused, 0);

    semaphore_wait(sem_evacuation, 0);
    data->evacuation = false;
    semaphore_signal(sem_evacuation, 0);

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
        // Jeśli flaga ewakuacji jest ustawiona na true...
        if (data->evacuation)
        {
            // Przeprowadź ewakucję
            evacuate_stadium(data, msgid_manager);
            break;
        }

        // Jeśli liczba kibiców osiągnęła maksymalną liczbę, nie ma ewakuacji
        // oraz wstrzymania wpuszczania kibiców
        if (data->fans_in_stadium >= MAX_FANS || data->evacuation == true && data->entry_paused == false)
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
                printf(WORKER "[TECH_WORKER(Main)] Stadion pełny i wszystkie stanowiska są puste. Kończymy pracę." RESET "\n");
                break; //...i kończymy pracę pracownika technicznego(main)
            }
        }
        sleep(1);
    }

    // Usuwanie mechanizmów IPC
    cleanup(data, msgid, shmid, msgid_manager, sem_fan_count, sem_evacuation, sem_entry_paused, sem_stands, sem_fans_waiting);
    printf(WORKER "[TECH_WORKER] Główny pracownik techniczny zakończył działanie" RESET "\n");
    exit(0);
}

void run_worker_stand(int stand_id)
{
    // Klucze IPC
    key_t key_msg = get_key(KEY_PATH, MSG_ID);
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_sem_stand = get_key(KEY_PATH, SEM_STAND_BASE);
    key_t key_sem_fans_waiting = get_key(KEY_PATH, SEM_WAITING_FANS);
    key_t key_fan_count = get_key(KEY_PATH, SEM_FAN_COUNT_ID);

    // Kolejka kibiców
    int msgid = create_message_queue(key_msg, IPC_CREAT | 0666);

    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0666);

    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Semafor liczby kibiców w kolejce
    int sem_fans_waiting = get_semaphore(key_sem_fans_waiting, 0666);

    // Semafor liczby kibiców na stadionie
    int sem_fan_count = get_semaphore(key_fan_count, 0666);

    printf(WORKER "[TECH_WORKER] Pracownik stanowiska %d uruchomiony" RESET "\n", stand_id + 1);

    // Kibice na stanowisku kontroli
    FanData stand_fans[MAX_PEOPLE_PER_STAND];

    for (int i = 0; i < MAX_PEOPLE_PER_STAND; i++)
    {
        reset_fan_data(&stand_fans[i]);
    }

    // Ilość kibiców do skontrolowania (zmniejszany, gdy kibic jest z innej drużyny)
    int stand_fan_id = 0;

    bool same_team = true;

    while (1)
    {
        // Odebrana wiadomość
        QueueMessage message;

        int invite_fans = 1;

        // Sprawdzenie czy kontrola jest wstrzymana
        if (data->entry_paused)
        {
            printf(WORKER "[TECH_WORKER] Stanowisko %d wstrzymane\n" RESET, stand_id + 1);
            sleep(1);
            continue;
        }

        // Sprawdzenie czy jest ewakuacja
        if (data->evacuation)
        {
            printf(WORKER "[TECH_WORKER] Stanowisko %d kończy pracę z powodu ewakuacji" RESET "\n", stand_id + 1);
            break;
        }

        // Sprawdzenie czy osiągnięto limit kibiców
        if (data->fans_in_stadium >= MAX_FANS)
        {
            printf(WORKER "[TECH_WORKER] Stanowisko %d kończy pracę: stadion pełny" RESET "\n", stand_id + 1);
            break;
        }

        // Sprawdzenie czy są jacyś kibice w kolejce
        if(get_sem_value(sem_fans_waiting) > 0)
        {
            // Odebranie wiadomości od kibica w kolejce
            if (msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), JOIN_CONTROL, IPC_NOWAIT) == -1)
            {
                if (msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), LET_FAN_GO, IPC_NOWAIT) == -1)
                {
                    if (msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), AGGRESSIVE_FAN, IPC_NOWAIT) == -1)
                    {
                        if (msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), VIP_ENTER, IPC_NOWAIT) == -1)
                        {
                            // sleep(1);
                            continue;
                        }
                    }
                }
            }
        }
        else
        {
            invite_fans = 0;
        }
        

        // Zapisujemy dane kibica, który chce wejść do kontroli
        stand_fans[stand_fan_id] = message.fData;

        // Sprawdzamy drużynę kibica jeśli nie jest pierwszym kibicem zaproszonym do kontroli
        // if (stand_fan_id > 0)
        // {
        same_team = check_fans_team(stand_fans[stand_fan_id].team, stand_fans, stand_fan_id);
        // }

        // Jeśli liczba kibiców do kontroli jest mniejsza od 3 oraz kibic jest
        // z tej samej drużyny oraz nie jest VIP
        printf("stand_fan_id(%d): %d\n", stand_id + 1, stand_fan_id);
        if (data->fans_on_stand[stand_id] < MAX_PEOPLE_PER_STAND && message.message_type != VIP_ENTER)
        {
            // Zwiększamy liczbę kibiców na stanowisku
            semaphore_wait(sem_fan_count, 0);
            (data->fans_on_stand[stand_id])++;
            semaphore_signal(sem_fan_count, 0);

            // Zwiększamy liczbę kibiców przystępujących do kontroli
            stand_fan_id++;

            printf("fans_waiting: %d\n", get_sem_value(sem_fans_waiting));
            printf("fans_on_stand(%d): %d\n", stand_id + 1, data->fans_on_stand[stand_id]);
            if (data->fans_on_stand[stand_id] < MAX_PEOPLE_PER_STAND && invite_fans == 1)
            {
                continue;
            }
        }
        // Jeśli kibic jest przeciwnej drużyny i nie jest VIP
        /*if (!same_team && message.message_type != VIP_ENTER)
        {
            message.message_type = OTHER_TEAM;
            message.sender = getpid();
            message.fData = stand_fans[stand_fan_id];

            // Wysyłamy wiadomość o tym że musi przepuścić kibica
            if (msgsnd(msgid, &message, sizeof(QueueMessage) - sizeof(long), 0) == -1)
            {
                perror(ERROR "Błąd wysłania wiadomości OTHER_TEAM do kibica" RESET);
                // semaphore_signal(control_sem);
            }
            stand_fan_id--;
            // reset_fan_data(&stand_fans[stand_fan_id]);
        }*/
        if (message.message_type == VIP_ENTER)
        {
            printf(WORKER "[TECH_WORKER] Kibic %d(VIP) - Wiek=%d, Team=%d, VIP=%d, Dziecko=%d, Poziom agresji=%d wchodzi bez kolejki\n" RESET, stand_fans[stand_fan_id].fan_id, stand_fans[stand_fan_id].age, stand_fans[stand_fan_id].team, stand_fans[stand_fan_id].is_vip, stand_fans[stand_fan_id].is_child, stand_fans[stand_fan_id].aggressive_counter);

            semaphore_wait(sem_fan_count, 0);
            (data->fans_in_stadium) += 1;
            semaphore_signal(sem_fan_count, 0);

            send_controlled_fan_message(data, &msgid, stand_fans, message, &stand_fan_id);

            continue;
        }
        if (data->fans_on_stand[stand_id] > 0)
        {
            control_fans(stand_fans, stand_id, &stand_fan_id);

            // Zwiększ liczbę kibiców na stadionie
            semaphore_wait(sem_fan_count, 0);
            (data->fans_in_stadium) += stand_fan_id;
            semaphore_signal(sem_fan_count, 0);

            data->fans_on_stand[stand_id] = 0;

            send_controlled_fan_message(data, &msgid, stand_fans, message, &stand_fan_id);

            stand_fan_id = 0;
        }

        // Odblokuj stanowisko
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

    sleep(2);

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