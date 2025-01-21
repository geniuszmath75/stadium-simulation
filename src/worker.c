#include "worker.h"
#include "ipc_utils.h"
#include "structures.h"

int sem_fan_count;
int sem_stand[MAX_STANDS];

void run_worker_main()
{
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_msg = get_key(KEY_PATH, MSG_ID);
    key_t key_sem_fan = get_key(KEY_PATH, SEM_FAN_COUNT_ID);

    int shmid = create_shared_memory(key_shm, sizeof(SharedData), IPC_CREAT | 0666);
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    int msgid = create_message_queue(key_msg, IPC_CREAT | 0666);
    sem_fan_count = create_semaphore(key_sem_fan, MAX_FANS, IPC_CREAT | 0666);

    for (int i = 0; i < MAX_STANDS; i++)
    {
        key_t key_sem_stand = get_key(KEY_PATH, SEM_STAND_BASE + i);
        sem_stand[i] = create_semaphore(key_sem_stand, MAX_PEOPLE_PER_STAND, IPC_CREAT | 0666);
    }

    data->fans_in_stadium = 0;
    for (int i = 0; i < MAX_STANDS; i++)
    {
        data->fans_on_stand[i] = 0;
    }

    printf(WORKER "[TECH_WORKER] Główny pracownik techniczny uruchomiony" RESET "\n");

    // Czekanie na sygnały lub maksymalną liczbę kibiców
    while (1)
    {
        // Logika odbierania i przetwarzania komunikatów od kierownika
        // lub sprawdzanie limitu kibiców na stadionie

        if (data->fans_in_stadium >= MAX_FANS)
        {
            bool all_stands_empty = true;
            for (int i = 0; i < MAX_STANDS; i++)
            {
                if (data->fans_on_stand[i] > 0) {
                    all_stands_empty = false;
                    break;
                }
            }
            if (all_stands_empty) {
                printf(WORKER "[TECH_WORKER] Stadion pełny i wszystkie stanowiska są puste. Kończymy pracę." RESET "\n");
                break;
            }
        }
        sleep(1);
    }

    // Usuwanie mechanizmów IPC
    detach_shared_memory(data);
    remove_shared_memory(shmid);
    remove_message_queue(msgid);
    remove_semaphore(sem_fan_count);
    for (int i = 0; i < MAX_STANDS; i++)
    {
        remove_semaphore(sem_stand[i]);
    }

    printf(WORKER "[TECH_WORKER] Główny pracownik techniczny zakończył działanie" RESET "\n");
    exit(0);
}

void run_worker_stand(int stand_id)
{
    key_t key_msg = get_key(KEY_PATH, MSG_ID);
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_sem_stand = get_key(KEY_PATH, SEM_STAND_BASE + stand_id);


    int msgid = create_message_queue(key_msg, 0);
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0);
    
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    int semid = create_semaphore(key_sem_stand, 1, 0);

    printf(WORKER "[TECH_WORKER] Pracownik stanowiska %d uruchomiony" RESET "\n", stand_id);

    FanData fan;
    while (1)
    {
        // Zablokuj stanowisko
        semaphore_wait(semid);

        if (data->fans_in_stadium >= MAX_FANS)
        {
            printf(WORKER "[TECH_WORKER] Stanowisko %d kończy pracę: stadion pełny" RESET "\n", stand_id);
            semaphore_signal(semid);
            break;
        }

        if (msgrcv(msgid, &fan, sizeof(FanData) - sizeof(long), 1, IPC_NOWAIT) == -1)
        {

            // Odblokuj stanowisko, jeśli brak kibiców w kolejce
            semaphore_signal(semid);
            if (data->fans_in_stadium >= MAX_FANS)
            {
                printf(WORKER "[TECH_WORKER] Stanowisko %d kończy pracę: brak kibiców w kolejce" RESET "\n", stand_id);
                break;
            }
            continue;
        }

        // Symulacja kontroli
        printf(WORKER "[TECH_WORKER] Stanowisko %d obsługuje kibica: ID=%d, Team=%d, VIP=%d, Child=%d, Age=%d" RESET "\n",
               stand_id, fan.fan_id, fan.team, fan.is_vip, fan.is_child, fan.age);
        sleep(1);

        // Zwiększ lcizbę kibiców na stadionie
        data->fans_in_stadium++;
        
        printf(WORKER "[TECH_WORKER] Stanowisko %d - Kibic: ID=%d, Team=%d, VIP=%d, Child=%d, Age=%d skontrolowany i wpuszczony na stadion" RESET "\n",
               stand_id, fan.fan_id, fan.team, fan.is_vip, fan.is_child, fan.age);
        
        // Zwiększ liczbę kibiców obsługiwanych przez stanowisko
        data->fans_on_stand[stand_id]++;

        // Zmiejsz liczbę kibiców obsługiwanych przez stanowisko
        data->fans_on_stand[stand_id]--;

        // Odblokuj stanowisko
        semaphore_signal(semid);
    }

    detach_shared_memory(data);
}
