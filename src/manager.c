#include <signal.h>
#include <fcntl.h>
#include <sys/select.h>
#include "manager.h"
#include "ipc_utils.h"
#include "structures.h"

// Wysłanie sygnału do pracownika technicznego
void send_signal_to_workers(int signal, pid_t main_tech_id)
{
    kill(main_tech_id, signal);
}

void run_manager(pid_t main_tech_id)
{
    // Klucze IPC
    key_t key_shm = get_key(KEY_PATH, SHM_ID);
    key_t key_msg_manager = get_key(KEY_PATH, MSG_MANAGER_ID);

    // Pamięc dzielona
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0);
    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    // Kolejka komunikatów do kierownika
    int msgid_manager = create_message_queue(key_msg_manager, 0);

    printf(MANAGER "[MANAGER] Kierownik uruchomiony.\n" RESET);

    while (1)
    {
        sleep(5);
        send_signal_to_workers(SIGTERM, main_tech_id);
        // Odczytywanie znaku z wejścia standardowego
        char command = getchar();

        // Wydawanie sygnałów na podstawie wprowadzonego znaku
        if (command == 'p')
        {
            if (data->entry_paused)
            {
                send_signal_to_workers(SIGUSR2, main_tech_id);
                printf(MANAGER "[MANAGER] Wstrzymać kibiców!\n" RESET);
            }
            else
            {
                send_signal_to_workers(SIGUSR1, main_tech_id);
                printf(MANAGER "[MANAGER] Ponownie wpuszczać kibiców!\n" RESET);
            }
        }
        else if (command == 'q')
        {
            send_signal_to_workers(SIGTERM, main_tech_id);
            printf(MANAGER "[MANAGER] Rozpoczęto ewakuację\n" RESET);
        }

        // Odbieranie komunikatu od pracownika technicznego
        QueueMessage msg;
        if (msgrcv(msgid_manager, &msg, sizeof(QueueMessage) - sizeof(long), EVACUATION_COMPLETE, 0) != -1)
        {
            printf(MANAGER "[MANAGER] Otrzymano komunikat o zakończeniu ewakuacji\n" RESET);
            break;
        }

        // Sprawdzanie, czy wszystkie procesy się zakończyły
        if (data->fans_in_stadium >= MAX_FANS && data->entry_paused == false && data->evacuation == false)
        {
            printf(MANAGER "[MANAGER] Stadion pełny. Kończymy działanie.\n" RESET);
            break;
        }
    }
    
    // Odłączenie pamięci dzielonej
    detach_shared_memory(data);
    printf(MANAGER "[MANAGER] Kierownik zakończył działanie\n" RESET);
    exit(0);
}
