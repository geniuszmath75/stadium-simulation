#include "fan.h"
#include "ipc_utils.h"
#include "structures.h"
#include <time.h>

FanData generate_random_fan(int id)
{
    FanData fan;
    fan.message_type = 1;                              // Ustawienie typu komunikatu
    fan.fan_id = id + 1;                               // ID użytkownika
    fan.team = rand() % 2 == 0 ? TEAM_A : TEAM_B;      // Drużyna
    fan.is_vip = rand() % 100 < (VIP_THRESHOLD * 100); // Czy VIP
    fan.age = rand() % 83 + 16;                        // Wiek
    fan.is_child = fan.age < 18 ? true : false;        // Czy dziecko
    return fan;
}

void run_fan(int id)
{
    // Losowe ziarno (do generowania danych kibica)
    srand(getpid());

    // Klucze IPC
    key_t key_msg = get_key(KEY_PATH, MSG_ID);
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_sem_fan_count = get_key(KEY_PATH, SEM_FAN_COUNT_ID);

    // Kolejka kibiców
    int msgid = msgget(key_msg, 0);
    if (msgid == -1) // Jeśli kolejka zwróci błąd to znaczy, że mechanizmy komunkicji zostały usunięte zatem stadion jest już pełny
    {
        printf(FAN "[FAN] Kibic %d nie może wejść: Kontrola zakończona\n" RESET, id + 1);
        exit(0);
    }

    // Semafor liczby kibiców
    int sem_fan_count = get_semaphore(key_sem_fan_count, 0);
    // Pamięć dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0);

    // Dołączenie pamięci dzielonej
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Generowanie danych kibica
    FanData fan = generate_random_fan(id);

    // Zablokowanie semafora liczby kibiców
    semaphore_wait(sem_fan_count);

    // Sprawdzenie czy jest ewakuacja
    if (data->evacuation)
    {
        printf(FAN "[FAN] Kibic %d nie może dołączyć do kolejki: Rozpoczęto ewakuację\n" RESET, id + 1);
        semaphore_signal(sem_fan_count); // Zwolnienie semafora
        detach_shared_memory(data); // Odłączenie pamięci dzielonej
        exit(0);
    }

    // Sprawdzenie limitu kibiców
    if (data->fans_in_stadium >= MAX_FANS)
    {
        printf(FAN "[FAN] Kibic %d nie może dołączyc do kolejki: Limit kibiców osiągnięty" RESET "\n", id + 1);
        semaphore_signal(sem_fan_count); // Zwolnienie semafora
        detach_shared_memory(data); // Odłączenie pamięci dzielonej
        exit(0);
    }

    // Wysłanie komunikatu o dołączeniu do kolejki
    if (msgsnd(msgid, &fan, sizeof(FanData) - sizeof(long), 0) != -1)
    {
        printf(FAN "[FAN] Kibic %d wysłał swoje dane: Wiek:%d, Team=%d, VIP=%d, Dziecko=%d\n" RESET,
               fan.fan_id, fan.age, fan.team, fan.is_vip, fan.is_child);
    }
    else
    {
        perror(ERROR "[FAN] Kibic nie mógł wysłać wiadomości\n" RESET);
        exit(1);
    }

    // Zwolnienie semafora
    semaphore_signal(sem_fan_count);
    exit(0);
}