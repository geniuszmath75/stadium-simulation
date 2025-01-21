#include "fan.h"
#include "ipc_utils.h"
#include "structures.h"
#include <time.h>

FanData generate_random_fan(int id) {
    FanData fan;
    fan.message_type = 1; // Ustawienie typu komunikatu
    fan.fan_id = id;
    fan.team = rand() % 2 == 0 ? TEAM_A : TEAM_B;
    fan.is_vip = rand() % 100 < (VIP_THRESHOLD * 100);
    fan.age = rand() % 83 + 16;
    fan.is_child = fan.age < 18 ? true : false;
    return fan;
}

void run_fan(int id) {
    srand(getpid());

    key_t key_msg = get_key(KEY_PATH, MSG_ID);
    key_t key_shm = get_key(KEY_PATH, SHM_ID);

    int msgid = create_message_queue(key_msg, 0);
    int shmid = create_shared_memory(key_shm, sizeof(SharedData), 0);

    SharedData *data = (SharedData *)attach_shared_memory(shmid);

    FanData fan = generate_random_fan(id);

    if(data->fans_in_stadium >= MAX_FANS) 
    {
        printf(FAN "[FAN] Kibic %d nie może wejść: Limit kibiców osiągnięty" RESET "\n", id);
        detach_shared_memory(data);
        exit(0);
    }
    if (msgsnd(msgid, &fan, sizeof(FanData) - sizeof(long), 0) == -1) {
        perror("msgsnd");
        exit(1);
    }

    printf(FAN "[FAN] Kibic %d wysłał swoje dane: Wiek:%d, Team=%d, VIP=%d, Child=%d\n" RESET,
           fan.fan_id, fan.age, fan.team, fan.is_vip, fan.is_child);
    exit(0);
}