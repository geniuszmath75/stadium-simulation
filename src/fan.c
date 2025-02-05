#include "fan.h"

FanData generate_random_fan(int id)
{
    FanData fan;
    fan.fan_pid = getpid();                             // PID kibica
    fan.fan_id = id;                                   // ID użytkownika
    fan.team = rand() % 2 == 0 ? TEAM_A : TEAM_B;      // Drużyna
    fan.is_vip = rand() % 100 < (VIP_THRESHOLD * 100); // Czy VIP
    fan.age = rand() % 83 + 16;                        // Wiek
    fan.is_child = fan.age < 18 ? true : false;        // Czy dziecko
    fan.aggressive_counter = TEST_AGGRESSIVE_FANS ? 5 : 0; // Agresywność kibica
    return fan;
}

void run_fan(int id)
{
    // Losowe ziarno (do generowania danych kibica)
    srand(getpid());

    // Klucze IPC
    key_t key_msg = get_key(KEY_PATH, MSG_ID);
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_fan_count = get_key(KEY_PATH, SEM_FAN_COUNT_ID);
    key_t key_sem_fans_waiting = get_key(KEY_PATH, SEM_WAITING_FANS);

    // Kolejka kibiców
    int msgid = msgget(key_msg, 0600);
    if (msgid == -1) // Jeśli kolejka zwróci błąd to znaczy, że mechanizmy komunkicji zostały usunięte zatem stadion jest już pełny
    {
        printf(FAN "[FAN] Kibic %d nie może wejść: Kontrola zakończona\n" RESET, id);
        exit(0);
    }

    // Semafor liczby kibiców na stadionie
    int sem_fan_count = get_semaphore(key_fan_count, 0600);

    // Semafor liczby kibiców w kolejce
    int sem_fans_waiting = get_semaphore(key_sem_fans_waiting, 0600);

    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0600);

    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Generowanie danych kibica
    FanData fan = generate_random_fan(id);

    // Wiadomość od kibica
    QueueMessage message = {
        .message_type = fan.is_vip ? VIP_ENTER : fan.aggressive_counter == 5 ? AGGRESSIVE_FAN : JOIN_CONTROL,
        .sender = getpid(),
        .fData = fan};

    while (1)
    {
        // Liczba kibiców w kolejce
        int fans_waiting = semctl(sem_fans_waiting, 0, GETVAL);

        if (fans_waiting == -1)
        {
            printf(FAN "[FAN] Kibic %d nie może dołączyć do kolejki: Stadion zamknięty\n" RESET, id);
            detach_shared_memory(data);
            exit(0);
        }

        // Sprawdzenie czy jest ewakuacja
        if (data->evacuation)
        {
            printf(FAN "[FAN] Kibic %d nie może dołączyć do kolejki: Rozpoczęto ewakuację\n" RESET, id);
            detach_shared_memory(data); // Odłączenie pamięci dzielonej
            exit(0);
        }

        // Sprawdzenie limitu kibiców
        if (fans_waiting == 0 && message.message_type == JOIN_CONTROL)
        {
            printf(FAN "[FAN] Kibic %d nie może dołączyc do kolejki: Limit kibiców osiągnięty" RESET "\n", id);
            detach_shared_memory(data); // Odłączenie pamięci dzielonej
            exit(0);
        }

        // Wysłanie komunikatu o dołączeniu do kolejki
        if (msgsnd(msgid, &message, sizeof(QueueMessage) - sizeof(long), 0) != -1)
        {
            switch (message.message_type)
            {
            case JOIN_CONTROL: // Dołączenie do kolejki do kontroli
                semaphore_wait(sem_fans_waiting, 0);
                printf(FAN "[FAN] Kibic %d dołączył do kolejki: Wiek:%d, Team=%d, VIP=%d, Dziecko=%d, Poziom agresji=%d\n" RESET,
                       fan.fan_id, fan.age, fan.team, fan.is_vip, fan.is_child, fan.aggressive_counter);
                break;
            case LET_FAN_GO: // Przepuszczenie kibica
                semaphore_wait(sem_fans_waiting, 0);
                printf(FAN "[FAN] Kibic %d przepuścił innego kibica w kolejce: Wiek:%d, Team=%d, VIP=%d, Dziecko=%d, Poziom agresji=%d\n" RESET,
                       fan.fan_id, fan.age, fan.team, fan.is_vip, fan.is_child, fan.aggressive_counter);
                break;
            case AGGRESSIVE_FAN: // Kibic agresywny
                semaphore_wait(sem_fans_waiting, 0);
                printf(FAN "[FAN] Kibic %d staje się agresywny: Wiek:%d, Team=%d, VIP=%d, Dziecko=%d, Poziom agresji=%d\n" RESET,
                       fan.fan_id, fan.age, fan.team, fan.is_vip, fan.is_child, fan.aggressive_counter);
                break;
            case VIP_ENTER: // Kibic VIP
                semaphore_wait(sem_fans_waiting, 0);
                printf(FAN "[FAN] Kibic %d(VIP) chce wejść na stadion: Wiek:%d, Team=%d, VIP=%d, Dziecko=%d, Poziom agresji=%d\n" RESET,
                       fan.fan_id, fan.age, fan.team, fan.is_vip, fan.is_child, fan.aggressive_counter);
                break;
            default:
                printf(FAN "break\n" RESET);
                break;
            }
        }
        else
        {
            perror(ERROR "[FAN] Kibic nie mógł wysłać wiadomości \n" RESET);
            exit(1);
        }

        while (1)
        {
            // Sprawdzenie czy jest ewakuacja
            if (data->evacuation)
            {
                printf(FAN "[FAN] Kibic %d wychodzi z kolejki: Rozpoczęto ewakuację\n" RESET, id);
                detach_shared_memory(data); // Odłączenie pamięci dzielonej
                exit(0);
            }
            // Oczekiwanie na odebranie informacji o odrzuceniu wpuszczenia do kontroli
            if (msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), OTHER_TEAM, IPC_NOWAIT) != -1)
            {
                if (message.fData.fan_pid == fan.fan_pid)
                {
                    semaphore_signal(sem_fans_waiting, 0);
                    fan.aggressive_counter++;
                    message.message_type = LET_FAN_GO;
                    if (fan.aggressive_counter > 5)
                    {
                        message.message_type = AGGRESSIVE_FAN;
                    }
                    break;
                }
                else
                {
                    message.message_type = OTHER_TEAM;
                    msgsnd(msgid, &message, sizeof(QueueMessage) - sizeof(long), 0);
                }
            }
            sleep(1);
            // Oczekiwanie na odebranie informacji o wpuszczeniu na stadion
            if ((msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), getpid(), IPC_NOWAIT) != -1) || errno == EIDRM)
            {
                break;
            }
        }
        // Jeśli typ komunikatu to PID kibica to przerwij pętlę(kibic wpuszczony na stadion)
        if (message.message_type == fan.fan_pid)
        {
            log_file(message.fData.fan_id, "entered_fans", "a+");

            // Zwiększanie liczby kibiców na stadionie
            semaphore_wait(sem_fan_count, 0);
            (data->fans_in_stadium) += 1;
            semaphore_signal(sem_fan_count, 0);

            printf(FAN "[FAN] Kibic %d wszedł na stadion\n" RESET, id);

            break;
        }
    }

    detach_shared_memory(data); // Odłączenie pamięci dzielonej
    exit(0);
}