#include "fan.h"
#include "ipc_utils.h"
#include "structures.h"
#include <time.h>

FanData generate_random_fan(int id)
{
    FanData fan;
    fan.fan_pid = getpid();
    fan.fan_id = id;                                   // ID użytkownika
    fan.team = rand() % 2 == 0 ? TEAM_A : TEAM_B;      // Drużyna
    fan.is_vip = rand() % 100 < (VIP_THRESHOLD * 100); // Czy VIP
    fan.age = rand() % 83 + 16;                        // Wiek
    fan.is_child = fan.age < 18 ? true : false;        // Czy dziecko
    fan.aggressive_counter = 0;
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
    int msgid = msgget(key_msg, 0666);
    if (msgid == -1) // Jeśli kolejka zwróci błąd to znaczy, że mechanizmy komunkicji zostały usunięte zatem stadion jest już pełny
    {
        printf(FAN "[FAN] Kibic %d nie może wejść: Kontrola zakończona\n" RESET, id);
        exit(0);
    }

    // Semafor liczby kibiców na stadionie
    int sem_fan_count = get_semaphore(key_fan_count, 0666);

    // Semafor liczby kibiców w kolejce
    int sem_fans_waiting = get_semaphore(key_sem_fans_waiting, 0666);

    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0666);

    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Generowanie danych kibica
    FanData fan = generate_random_fan(id);

    // Wiadomość od kibica
    QueueMessage message = {
        .message_type = fan.is_vip ? VIP_ENTER : JOIN_CONTROL,
        .sender = getpid(),
        .fData = fan};

    while (1)
    {
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
        if (fans_waiting == 0)
        {
            printf(FAN "[FAN] Kibic %d nie może dołączyc do kolejki: Limit kibiców osiągnięty" RESET "\n", id);
            detach_shared_memory(data); // Odłączenie pamięci dzielonej
            exit(0);
        }

        // Zapewnienie odpowiedniej kolejności wchodzenia kibiców
        // if (fan.fan_id + fans_waiting != MAX_FANS + 1)
        // {
        //     continue;
        // }

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
                printf(FAN "[FAN] Kibic %d przepuścił innego kibica w kolejce: Wiek:%d, Team=%d, VIP=%d, Dziecko=%d, Poziom agresji=%d\n" RESET,
                       fan.fan_id, fan.age, fan.team, fan.is_vip, fan.is_child, fan.aggressive_counter);
                break;
            case AGGRESSIVE_FAN: // Kibic agresywny
                printf(FAN "[FAN] Kibic %d staje się agresywny: Wiek:%d, Team=%d, VIP=%d, Dziecko=%d, Poziom agresji=%d\n" RESET,
                       fan.fan_id, fan.age, fan.team, fan.is_vip, fan.is_child, fan.aggressive_counter);
                break;
            case VIP_ENTER: // Kibic VIP
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
            // Oczekiwanie na odebranie informacji o odrzuceniu wpuszczenia do kontroli
            if (msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), OTHER_TEAM, IPC_NOWAIT) != -1 && (message.fData.fan_pid == fan.fan_pid))
            {
                fan.aggressive_counter++;
                message.message_type = LET_FAN_GO;
                if (fan.aggressive_counter > 5)
                {
                    message.message_type = AGGRESSIVE_FAN;
                }
                break;
            }
            // Oczekiwanie na odebranie informacji o wpuszczeniu na stadion
            if ((msgrcv(msgid, &message, sizeof(QueueMessage) - sizeof(long), getpid(), IPC_NOWAIT) != -1))
            {
                printf(FAN "[FAN] Kibic %d wszedł na stadion\n" RESET, id);
                break;
            }
        }
        // Jeśli typ komunikatu to PID kibica to przerwij pętlę(kibic wpuszczony na stadion)
        if (message.message_type == fan.fan_pid)
        {
            break;
        }
    }

    detach_shared_memory(data); // Odłączenie pamięci dzielonej
    exit(0);
}