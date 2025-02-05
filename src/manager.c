#include "manager.h"

// Wysłanie sygnału do pracownika technicznego
void send_signal_to_workers(int signal, pid_t main_tech_id)
{
    kill(main_tech_id, signal);
}

void run_manager(pid_t main_tech_id)
{
    srandom(getpid());
    // Klucze IPC
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_msg_manager = get_key(KEY_PATH, MSG_MANAGER_ID);

    // Pamięc dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0600);
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Kolejka komunikatów do kierownika
    int msgid_manager = create_message_queue(key_msg_manager, 0600);

    printf(MANAGER "[MANAGER] Kierownik uruchomiony.\n" RESET);

    // Czas oczekiwania na wysłanie sygnału
    int wait_for_signal = TEST_SIGNAL_HANDLING == 1 ? rand() % MAX_FANS + 5 : TEST_SIGNAL_HANDLING == 0 ? (MAX_FANS - 4) + rand() % 4 : (int)ceil(log2((double)MAX_FANS));
    int total_sleep_time = 0;

    while (1)
    {
        if (total_sleep_time + wait_for_signal > MAX_FANS)
            wait_for_signal = MAX_FANS - total_sleep_time;

        sleep(wait_for_signal);
        total_sleep_time += wait_for_signal;

        if (TEST_SIGNAL_HANDLING == 0)
            send_signal_to_workers(SIGTERM, main_tech_id);
        if (TEST_SIGNAL_HANDLING == 1)
        {
            send_signal_to_workers(SIGUSR1, main_tech_id);
            sleep(3); // Wstrzymanie kontroli na 3s
            total_sleep_time += 3;
            send_signal_to_workers(SIGUSR2, main_tech_id);
        }

        if (TEST_SIGNAL_HANDLING == 0)
        {
            // Odbieranie komunikatu od pracownika technicznego
            QueueMessage msg;
            if (msgrcv(msgid_manager, &msg, sizeof(QueueMessage) - sizeof(long), EVACUATION_COMPLETE, 0) != -1)
            {
                printf(MANAGER "[MANAGER] Otrzymano komunikat o zakończeniu ewakuacji\n" RESET);
            }
            msg.message_type = FINISH_WORKER;
            if (msgsnd(msgid_manager, &msg, sizeof(QueueMessage) - sizeof(long), 0) == -1)
            {
                printf(MANAGER "[MANAGER] Błąd wysłania wiadomości FINISH_WORKER\n" RESET);
            }
            break;
        }

        // Sprawdzanie, czy wszystkie procesy się zakończyły
        if (data->fans_in_stadium >= MAX_FANS && data->entry_paused == false && data->evacuation == false)
        {
            break;
        }
        if (total_sleep_time >= MAX_FANS)
            break;
    }

    // Odłączenie pamięci dzielonej
    detach_shared_memory(data);
    printf(MANAGER "[MANAGER] Kierownik zakończył działanie\n" RESET);
    exit(0);
}
